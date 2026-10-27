///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ZombiesGame.cpp
// Purpose: The simulation of a Zombies level - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ZombiesGame.h"
#include "Components.h"
#include "topdown/Height.h"

#include "box2D/PhysicsWorld2D.h"
#include "gameplay/NavGrid2D.h"
#include "LogMgr.h"
#include "Profiler.h"

#include <algorithm>
#include <cmath>

namespace Zombies
{
	namespace
	{
		const float kBulletTraceSeconds = 0.05f; // a bullet that hit something stays drawn this long
		const float kTeleportInterval = 0.1f;    // seconds between teleports of zombies left behind (EnemyTeleportInterval)
		const float kTeleportStep = 1.0f;        // meters between the spots tried along the screen edge
		const float kStrikeRadius = 4.5f;        // meters of each blast of an air strike
		const float kStrikeDamage = 500.0f;      // the game's explosion damage
		const float kEnemyShotHitRadius = 0.5f;  // meters around the player where an enemy shot hits (half its 100 cm body)
		const float kPropHitRadius = 0.5f;       // a prop's size for a swing
		// Money left on the floor is not lost: after a while a zombie that sees it (this close) comes, takes it and runs
		// off with it for some seconds before it chases again; killing it gets the money back. Nobody takes it: it goes
		// after a minute.
		const float kCoinCallSeconds = 3.0f;
		const float kCoinCallInterval = 0.5f;
		const float kCoinCallRange = 14.0f;
		const float kCoinLifeSeconds = 60.0f;
	}

	ZombiesGame::ZombiesGame(Pakal::PhysicsWorld2D& world, Pakal::NavGrid2D& nav, const GameRules& rules, uint32_t seed) :
		m_world(world),
		m_nav(nav),
		m_rules(rules),
		m_random(seed)
	{
		factions.set(survivors, undead, Relation::Hostile);
	}

	ZombiesGame::~ZombiesGame()
	{
		remove_all();
	}

	float ZombiesGame::random01()
	{
		return m_random["sim"].uniform();
	}

	bool ZombiesGame::frozen() const
	{
		const HealthComponent* health = player() ? player()->get<HealthComponent>() : nullptr;
		return health && health->is_dead();
	}

	void ZombiesGame::step(float dt, std::span<const PlayerInput> inputs)
	{
		advance(dt, inputs);
		events.deliver();
	}

	void ZombiesGame::advance(float dt, std::span<const PlayerInput> inputs)
	{
		take_inputs(inputs);
		timeMs += dt * 1000.0;
		join_added();
		if (frozen())
		{
			player()->previousPosition = player()->position;
			player()->update(*this, dt);
			return;
		}
		remember_positions();
		update_entities(dt);
		{
			PAKAL_PROFILE_SCOPE("sim.physics");
			m_world.step(dt);
		}
		teleport_enemies(dt);
		step_bullets(dt);
		for (auto& strike : m_strikes)
		{
			strike.delay -= dt;
			if (strike.delay <= 0.0f)
			{
				play(m_rules.sound("strikeBlast"), strike.at);
				blast(strike.at, kStrikeRadius, kStrikeDamage, 0, m_rules.shake.bombing);
				strike.delay = 1.0e9f;
				strike.at = b2Vec2(1.0e9f, 1.0e9f); // done
			}
		}
		m_strikes.erase(std::remove_if(m_strikes.begin(), m_strikes.end(), [](const Strike& s) { return s.at.x > 1.0e8f; }), m_strikes.end());
		join_added();
		remove_removed();
	}

	void ZombiesGame::entity_removed(Actor& actor)
	{
		m_coinCalls.erase(actor.id);
		TopDownGame::entity_removed(actor);
	}

	void ZombiesGame::fire_bullet(const b2Vec2& from, const b2Vec2& direction, float speed, float damage, float range, float piercing, bool knockback, int faction, float height)
	{
		Bullet bullet;
		bullet.faction = faction;
		bullet.height = height;
		bullet.piercing = piercing;
		bullet.knockback = knockback;
		bullet.position = from;
		bullet.direction = direction;
		bullet.speed = speed;
		bullet.damage = damage;
		bullet.secondsLeft = speed > 0.0f ? range / speed : 0.0f;
		m_bullets.push_back(bullet);
	}

	void ZombiesGame::fire_enemy_bullet(const b2Vec2& from, const b2Vec2& direction, float speed, float damage, float range)
	{
		fire_bullet(from, direction, speed, damage, range, 1.0f, false, undead);
		m_bullets.back().fromEnemy = true;
	}

	void ZombiesGame::enemy_blast(const b2Vec2& at, float radius, float amount, uintptr_t source)
	{
		std::vector<Actor*> hit;
		for (auto& actor : entities())
		{
			if (actor->id != source && !actor->removed && (actor->position - at).Length() < radius
				&& (actor->get<EnemyBrainComponent>() || actor.get() == player()))
			{
				hit.push_back(actor.get());
			}
		}
		for (Actor* actor : hit)
		{
			damage(*actor, { .amount = amount, .type = DamageType::Blast, .source = source, .from = at });
		}
	}

	// Bullets fly ProjectileBullet.Speed and stop at the first wall, zombie or prop on their way. An enemy's shot stops at
	// the first wall or at the player.
	void ZombiesGame::step_bullets(float dt)
	{
		for (auto& bullet : m_bullets)
		{
			if (bullet.hit)
			{
				bullet.secondsLeft = -1.0f; // its last frame was drawn
				continue;
			}
			bullet.secondsLeft -= dt;
			const b2Vec2 to = bullet.position + (bullet.speed * dt) * bullet.direction;
			Pakal::RayCastHit hit = m_world.raycast(bullet.position, to, layers.wall);
			// Zombies are hit on a circle as wide as their model (EnemyRules::hit_radius), not on the thin body they walk with
			const b2Vec2 path = to - bullet.position;
			const float pathLength = path.Length();
			float nearest = hit.hit ? (hit.point - bullet.position).Length() : pathLength;
			if (bullet.fromEnemy)
			{
				if (player() && pathLength > 0.0f && body_at(*player(), bullet.height))
				{
					const b2Vec2 toCenter = player()->position - bullet.position;
					const float along = b2Dot(toCenter, bullet.direction);
					const float sideSquared = toCenter.LengthSquared() - along * along;
					const float radius = kEnemyShotHitRadius;
					if (sideSquared <= radius * radius)
					{
						const float entry = std::max(0.0f, along - std::sqrt(radius * radius - sideSquared));
						if (along >= -radius && entry < nearest)
						{
							nearest = entry;
							hit.hit = true;
							hit.point = bullet.position + entry * bullet.direction;
							hit.userData = player()->id;
						}
					}
				}
			}
			for (const auto& actor : entities())
			{
				if (bullet.fromEnemy) break;
				const EnemyBrainComponent* brain = actor->removed ? nullptr : actor->get<EnemyBrainComponent>();
				if (!brain || brain->is_dead() || pathLength <= 0.0f || !body_at(*actor, bullet.height)
					|| std::find(bullet.pierced.begin(), bullet.pierced.end(), actor->id) != bullet.pierced.end())
				{
					continue;
				}
				const float radius = brain->rules().hit_radius();
				const b2Vec2 toCenter = actor->position - bullet.position;
				const float along = b2Dot(toCenter, bullet.direction);
				const float sideSquared = toCenter.LengthSquared() - along * along;
				if (sideSquared > radius * radius)
				{
					continue;
				}
				const float entry = along - std::sqrt(radius * radius - sideSquared);
				if (entry >= -radius && entry < nearest)
				{
					nearest = std::max(0.0f, entry);
					hit.hit = true;
					hit.point = bullet.position + nearest * bullet.direction;
					hit.userData = actor->id;
				}
			}
			if (!hit.hit)
			{
				bullet.position = to;
				continue;
			}
			Actor* target = hit.userData != 0 ? find(hit.userData) : nullptr;
			// A piercing bullet goes on through a zombie while some of its piercing is left
			// the characters hostile to the shooter (the zombies, for the player's bullets)
			const bool foe = target && target->kind == ActorKind::Character && factions.hostile(bullet.faction, faction_of(*target));
			if (foe && bullet.piercingLeft - bullet.piercing > 0.0f)
			{
				bullet.piercingLeft -= bullet.piercing;
				bullet.pierced.push_back(target->id);
				events.emit(ImpactEvent{ hit.point, bullet.direction, ImpactKind::Enemy });
				damage(*target, { .amount = bullet.damage, .type = DamageType::Bullet, .from = player() ? player()->position : hit.point - bullet.direction, .knockback = bullet.knockback });
				bullet.position = hit.point;
				continue;
			}
			bullet.position = hit.point;
			bullet.hit = true;
			bullet.secondsLeft = kBulletTraceSeconds;
			events.emit(ImpactEvent{ hit.point, bullet.direction, !target ? ImpactKind::Wall : (foe ? ImpactKind::Enemy : ImpactKind::Prop) });
			if (target)
			{
				damage(*target, { .amount = bullet.damage, .type = DamageType::Bullet, .from = player() ? player()->position : hit.point - bullet.direction, .knockback = bullet.knockback });
			}
		}
		m_bullets.erase(std::remove_if(m_bullets.begin(), m_bullets.end(),
			[](const Bullet& bullet) { return bullet.secondsLeft <= 0.0f; }), m_bullets.end());
	}

	void ZombiesGame::set_screen(const b2Vec2& minCorner, const b2Vec2& maxCorner)
	{
		m_hasScreen = true;
		m_screenMin = minCorner;
		m_screenMax = maxCorner;
	}

	bool ZombiesGame::on_screen(const b2Vec2& p) const
	{
		return !m_hasScreen || (p.x >= m_screenMin.x && p.x <= m_screenMax.x && p.y >= m_screenMin.y && p.y <= m_screenMax.y);
	}

	bool ZombiesGame::far_off_screen(const b2Vec2& p) const
	{
		if (!m_hasScreen || !player() || on_screen(p))
		{
			return false;
		}
		const b2Vec2 at = player()->position;
		float corner = 0.0f;
		for (const b2Vec2& c : { m_screenMin, m_screenMax, b2Vec2(m_screenMin.x, m_screenMax.y), b2Vec2(m_screenMax.x, m_screenMin.y) })
		{
			corner = std::max(corner, (c - at).Length());
		}
		return (p - at).Length() > 2.0f * corner;
	}

	// EnemyManagementSystem::TeleportEnemies: every 0.1 s while the player walks, one chasing zombie off screen and not
	// ahead of the player is moved to the screen edge the player walks toward, at the first free spot from the player's
	// line outward in 1 m steps
	void ZombiesGame::teleport_enemies(float dt)
	{
		m_teleportTimer += dt;
		// behind barricades (a defend mission) they have to come through them
		if (!m_hasScreen || !player() || m_teleportTimer < kTeleportInterval || !barricades.empty())
		{
			return;
		}
		b2Vec2 walking = input().move;
		if (walking.Normalize() < 0.0001f)
		{
			return;
		}
		const b2Vec2 at = player()->position;
		Actor* mover = nullptr;
		for (auto& actor : entities())
		{
			EnemyBrainComponent* brain = actor->removed ? nullptr : actor->get<EnemyBrainComponent>();
			if (!brain || brain->is_dead() || brain->is_attacking() || brain->knocked_back() || on_screen(actor->position))
			{
				continue;
			}
			b2Vec2 toEnemy = actor->position - at;
			if (toEnemy.Normalize() > 0.0f && b2Dot(walking, toEnemy) > 0.6f)
			{
				continue;
			}
			mover = actor.get();
			break;
		}
		if (!mover)
		{
			return;
		}
		const bool alongX = std::fabs(walking.x) > std::fabs(walking.y);
		const float edge = alongX ? (walking.x > 0.0f ? m_screenMax.x : m_screenMin.x) : (walking.y > 0.0f ? m_screenMax.y : m_screenMin.y);
		const float start = alongX ? at.y : at.x;
		const float limit = alongX ? m_screenMax.y : m_screenMax.x;
		for (int i = 0; i < 200; ++i)
		{
			const float offset = (i % 2 ? 1.0f : -1.0f) * static_cast<float>(i) * kTeleportStep;
			const float across = start + offset;
			if (across > limit + kTeleportStep * 2.0f) return;
			const b2Vec2 spot = alongX ? b2Vec2(edge, across) : b2Vec2(across, edge);
			if (!m_nav.is_walkable(spot)) continue;
			bool free = true;
			for (const auto& other : entities())
			{
				if (other.get() != mover && !other->removed && (other->position - spot).LengthSquared() < 1.0f) { free = false; break; }
			}
			if (!free) continue;
			mover->get<EnemyBrainComponent>()->teleport(spot);
			mover->position = spot;
			mover->previousPosition = spot;
			m_teleportTimer = 0.0f;
			return;
		}
	}

	DamageResult ZombiesGame::apply_damage(Actor& target, const DamageInfo& hit)
	{
		DamageResult result;
		if (EnemyBrainComponent* brain = target.get<EnemyBrainComponent>())
		{
			if (brain->is_dead()) return result;
			result.hit = true;
			result.taken = brain->damage(*this, hit);
			result.killed = brain->is_dead();
			return result;
		}
		if (DestructibleComponent* prop = target.get<DestructibleComponent>())
		{
			if (prop->is_broken()) return result;
			const float before = prop->health();
			prop->damage(*this, hit.amount);
			result.hit = true;
			result.taken = before - prop->health();
			result.killed = prop->is_broken();
			return result;
		}
		if (HealthComponent* health = target.get<HealthComponent>())
		{
			const float taken = health->damage(hit.amount);
			if (taken > 0.0f && &target == player())
			{
				play(m_rules.player.hurtSound, target.position);
				if (health->is_dead())
				{
					LOG_INFO("[Zombies] the player is down");
					play(m_rules.sound("playerDown"), target.position);
				}
			}
			result.hit = taken > 0.0f;
			result.taken = taken;
			result.killed = result.hit && health->is_dead();
		}
		return result;
	}

	// ExplosiveComponent: the blast hurts enemies and level objects (not the player), and does not set
	// off other explosives
	void ZombiesGame::blast(const b2Vec2& at, float radius, float amount, uintptr_t source, float shake)
	{
		std::vector<Actor*> hit;
		for (auto& actor : entities())
		{
			if (actor->id == source || actor->removed || (actor->position - at).Length() > radius)
			{
				continue;
			}
			const DestructibleComponent* prop = actor->get<DestructibleComponent>();
			if (actor->get<EnemyBrainComponent>() || (prop && !prop->explosive() && !prop->rules().repairable && !prop->rules().objective))
			{
				hit.push_back(actor.get());
			}
		}
		for (Actor* actor : hit)
		{
			damage(*actor, { .amount = amount, .type = DamageType::Blast, .source = source, .from = at });
		}
		events.emit(ExplosionEvent{ at, radius, shake < 0.0f ? m_rules.shake.explosion : shake });
	}

	void ZombiesGame::roll_drop(bool random, const std::vector<int>& drops, float rate, const b2Vec2& at)
	{
		const float roll = random01();
		const float pick = random01();
		const int pickup = m_rules.pick_drop(random, drops, rate, roll, pick);
		if (pickup != 0)
		{
			drop_pickup(pickup, at);
		}
	}

	void ZombiesGame::drop_pickup(int pickupId, const b2Vec2& at)
	{
		auto found = m_rules.pickups.find(pickupId);
		if (found == m_rules.pickups.end())
		{
			return;
		}
		Actor& pickup = add_actor(ActorKind::Pickup, found->second.type);
		pickup.gameId = pickupId;
		pickup.position = at;
		pickup.add<PickupComponent>(found->second, found->second.type == "Money" ? kCoinLifeSeconds : 0.0f);
	}

	bool ZombiesGame::take_pickup(Actor& taker, const PickupRules& rules)
	{
		(void)taker;
		if (rules.type == "Money")
		{
			money += static_cast<int>(rules.value);
			return true;
		}
		if (rules.type == "Token")
		{
			tokens += static_cast<int>(rules.value);
			return true;
		}
		return false;
	}

	void ZombiesGame::pickup_lying(Actor& pickup, float lyingSeconds, float dt)
	{
		const PickupComponent* component = pickup.get<PickupComponent>();
		if (!component || component->rules().type != "Money" || lyingSeconds < kCoinCallSeconds)
		{
			return;
		}
		float& timer = m_coinCalls[pickup.id];
		if ((timer -= dt) <= 0.0f)
		{
			timer = kCoinCallInterval;
			call_a_zombie(pickup);
		}
	}

	void ZombiesGame::call_a_zombie(Actor& coin)
	{
		EnemyBrainComponent* nearest = nullptr;
		float best = kCoinCallRange;
		for (const auto& other : entities())
		{
			EnemyBrainComponent* brain = other->get<EnemyBrainComponent>();
			if (!brain || other->removed)
			{
				continue;
			}
			if (brain->coin() == coin.id)
			{
				return;   // one is on its way already
			}
			const float distance = (other->position - coin.position).Length();
			if (distance < best && brain->free_for_coin() && brain->sees(*this, coin.position))
			{
				best = distance;
				nearest = brain;
			}
		}
		if (nearest) nearest->go_for_coin(coin.id);
	}

	bool ZombiesGame::apply_effect(const std::string& effect, Actor& user)
	{
		if (effect == "airstrike") // not a stat but a bombardment (AirStrikeBoost)
		{
			airstrike(user.position);
			LOG_INFO("[Zombies] air strike called");
			return true;
		}
		return false;
	}

	bool ZombiesGame::is_alive(const Actor& actor) const
	{
		if (actor.removed) return false;
		if (const EnemyBrainComponent* brain = actor.get<EnemyBrainComponent>()) return !brain->is_dead();
		if (const DestructibleComponent* prop = actor.get<DestructibleComponent>()) return !prop->is_broken();
		if (const HealthComponent* health = actor.get<HealthComponent>()) return !health->is_dead();
		return true;
	}

	float ZombiesGame::hit_radius(const Actor& actor) const
	{
		const EnemyBrainComponent* brain = actor.get<EnemyBrainComponent>();
		return brain ? meters(brain->rules().collisionSize) * 0.5f : kPropHitRadius;
	}

	void ZombiesGame::airstrike(const b2Vec2& center)
	{
		const int count = 9;
		for (int i = 0; i < count; ++i)
		{
			// Spread over a ring around the player, never on top of it
			const float angle = random01() * 6.2831853f;
			const float distance = 3.5f + random01() * 7.5f;
			m_strikes.push_back({ 0.5f + 0.22f * static_cast<float>(i), center + distance * b2Vec2(std::cos(angle), std::sin(angle)) });
		}
	}

	void ZombiesGame::enemy_killed(Actor& enemy)
	{
		(void)enemy;
		++kills;
	}

	int ZombiesGame::alive_enemies() const
	{
		int alive = 0;
		auto count = [&alive](const std::vector<std::unique_ptr<Actor>>& actors)
		{
			for (const auto& actor : actors)
			{
				const EnemyBrainComponent* brain = actor->get<EnemyBrainComponent>();
				if (brain && !brain->is_dead() && !actor->removed) ++alive;
			}
		};
		count(entities());
		count(added());
		return alive;
	}

	void ZombiesGame::set_berserk(bool on)
	{
		berserk = on;
		for (auto& actor : entities())
		{
			if (EnemyBrainComponent* brain = actor->get<EnemyBrainComponent>())
			{
				brain->set_berserk(on);
			}
		}
	}

	void ZombiesGame::kill_all_enemies()
	{
		for (auto& actor : entities())
		{
			if (EnemyBrainComponent* brain = actor->get<EnemyBrainComponent>())
			{
				brain->kill(*this);
			}
		}
	}
}
