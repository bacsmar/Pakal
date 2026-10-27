///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Components.cpp
// Purpose: The components of the Zombies actors - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Components.h"
#include "topdown/ActorOrientation.h"
#include "ZombiesGame.h"

#include "box2D/PhysicsWorld2D.h"
#include "topdown/EnemyAI2D.h"
#include "gameplay/NavGrid2D.h"
#include "LogMgr.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace Zombies
{
	namespace
	{
		const float kSlowDownDistance = 10.0f;  // meters: closer to the player zombies walk slower (gamesettings SlowDownDistance)
		const float kSlowDownSpeed = 0.8f;
		const float kHurtFlashSeconds = 0.15f;  // the red flash of a hurt body (HealthComponent)
		const float kHurtClipInterval = 2.0f;   // a zombie's hurt clip plays at most this often (HealthComponent)
		const float kHealthBarSeconds = 2.0f;   // a zombie's health bar shows this long after a hit (GameUI)
		const float kBomberCloseIn = 1.0f;      // a tourist about to blow up runs at the player until this close (m)
		const float kShotStart = 0.475f;         // a boomer's shots leave this far out (half the bullet's length)
		const float kPlayerBodyRadius = 0.5f;    // the player's body in the original (100 cm): a dog touching it bites
		// A flung body (HealthComponent FLING_DEATH): MeleeDeathHoriSpeed 300 and MeleeDeathVertSpeed 1500 cm/s under the
		// gravity of the original's death physics, and it stays at least 3 s
		const float kFlingSpeed = 3.0f;
		const float kFlingLift = 15.0f;
		const float kFlingGravity = 9.8f;
		const float kFlingTumble = 7.0f;         // radians per second it turns over while flying
		const float kFlungStaySeconds = 3.0f;
		// Money on the floor (ZombiesGame::pickup_lying calls a zombie for it): it walks up to it, takes it and runs off with
		// it for some seconds before it chases again
		const float kCoinReach = 1.0f;          // it stops this close to what it walks up to (its stop distance, ~0.7 m)
		const float kCoinGiveUpSeconds = 10.0f;
		const float kCoinFleeSeconds = 4.0f;
		const float kCoinFleeSpeed = 1.3f;
		// Defend missions: a zombie next to a barricade standing between it and its target breaks it first; most zombies go
		// for the thing to defend unless the player is this close to them
		const float kBarricadeNear = 1.6f;
		const float kObjectiveIgnoreNear = 4.0f;
		const float kActorRadius = 0.3f;         // our characters' bodies (meters)
		const float kEnemySight = 40.0f;         // spawned zombies always know where the player is
	}

	// EnemyBrainComponent --------------------------------------------------------------------------

	EnemyConfig EnemyBrainComponent::config_for(const EnemyRules& rules, bool berserk, int difficulty, const TopDownLayers& layers)
	{
		EnemyConfig config;
		config.bodyCategory = layers.enemy;
		config.sightBlockers = layers.wall;
		config.radius = kActorRadius;
		config.sightRange = kEnemySight;
		config.pursueTarget = true;
		const EnemyRules::Row row = rules.at(difficulty, berserk);
		config.speed = meters(row.speed);
		config.maxHealth = row.health;
		config.attackDamage = row.attackDamage;
		// The game measures the reach from the centers of bodies 'collisionSize' wide; ours are thinner,
		// so the same gap between the bodies is kept
		config.attackRange = std::max(2.0f * kActorRadius, meters(rules.attackRange - rules.collisionSize) + 2.0f * kActorRadius);
		config.attackWindup = rules.attackDelayMs / 1000.0f;
		// As the original: the attack clip plays at its own speed (the hit lands DamageDelay into it), the zombie stands
		// until it ends, and only then the wait between attacks starts. Between attacks it keeps closing in until it touches
		// the player, and turns toward it at RotateSpeed first.
		config.attackDuration = rules.attackClipMs > 0.0f ? rules.attackClipMs / 1000.0f
			: (rules.flash ? std::max(rules.attackTimeMs, rules.attackDelayMs) / 1000.0f : config.attackWindup);
		config.attackCooldown = rules.attackIntervalMinMs / 1000.0f;
		config.attackCooldownMax = rules.attackIntervalMaxMs / 1000.0f;
		config.stopDistance = 2.0f * kActorRadius + 0.05f;
		config.turnDegreesPerSecond = rules.rotateSpeed;
		if (rules.flash)
		{
			config.attackArcDegrees = 2.0f * rules.attackAngle; // AttackRangeAngle is each side of the facing
		}
		if (rules.attack != EnemyAttack::Melee && rules.attack != EnemyAttack::Flash)
		{
			// The special attacks run in the brain (update_special). As the original measures them: from center to center,
			// it walks up to AttackRange and may start from AttackRadius; a dog starts as soon as it faces the player.
			config.scriptedAttack = true;
			config.attackRange = meters(rules.attackRange);
			config.stopDistance = config.attackRange;
			config.attackStartRange = rules.attack == EnemyAttack::Dog ? 1.0e6f : meters(rules.attackRadius > 0.0f ? rules.attackRadius : rules.attackRange);
			config.attackWindup = 0.0f;
			config.attackDuration = 0.0f;
		}
		return config;
	}

	namespace
	{
		// The bear standing: the chain's reach, damage and waits instead of the swat's
		EnemyConfig reared_config(EnemyConfig config, const EnemyRules& rules)
		{
			config.attackRange = meters(rules.chainRange);
			config.stopDistance = config.attackRange;
			config.attackStartRange = meters(rules.chainRadius);
			config.attackDamage = rules.chainDamage;
			config.attackCooldown = rules.chainIntervalMinMs / 1000.0f;
			config.attackCooldownMax = rules.chainIntervalMaxMs / 1000.0f;
			return config;
		}
	}

	EnemyBrainComponent::EnemyBrainComponent(ZombiesGame& game, const std::string& type, const b2Vec2& at, uintptr_t id) :
		m_type(type),
		m_layers(game.layers),
		m_rules(&game.rules().enemy(type)),
		m_berserk(game.berserk),
		m_difficulty(game.difficulty)
	{
		m_ai = std::make_unique<EnemyAI2D>(game.world(), at, config_for(*m_rules, m_berserk, m_difficulty, m_layers), id);
		if (m_rules->attack == EnemyAttack::Bear && m_rules->sleepMs > 0.0f)
		{
			m_phase = Phase::Sleep;   // it comes asleep
			show("sleep", 0.0f, false);
		}
	}

	EnemyBrainComponent::~EnemyBrainComponent() = default;

	bool EnemyBrainComponent::is_dead() const { return m_ai->is_dead(); }
	bool EnemyBrainComponent::is_attacking() const { return m_ai->is_attacking(); }
	bool EnemyBrainComponent::attack_started() const { return m_ai->attack_started(); }
	b2Vec2 EnemyBrainComponent::attack_direction() const { return m_ai->get_attack_direction(); }
	b2Vec2 EnemyBrainComponent::position() const { return m_ai->get_position(); }
	b2Vec2 EnemyBrainComponent::facing() const { return m_ai->get_facing(); }

	void EnemyBrainComponent::set_berserk(bool on)
	{
		if (on == m_berserk || m_ai->is_dead())
		{
			return;
		}
		m_berserk = on;
		m_ai->set_config(config_for(*m_rules, on, m_difficulty, m_layers), true); // Level::ToggleBerserkMode keeps the health share
	}

	float EnemyBrainComponent::damage(ZombiesGame& game, const DamageInfo& hit)
	{
		if (m_ai->is_dead() || bear_still())
		{
			return 0.0f;   // dead, or a bear asleep, waking or rearing up
		}
		float amount = hit.amount;
		const bool knockback = hit.knockback;
		const b2Vec2& from = hit.from;
		const DeathKind death = hit.death;
		if (m_reared) amount *= m_rules->abdomenMul;   // the bear standing shows its belly
		m_ai->apply_damage(amount);
		m_hurtTimer = kHurtFlashSeconds;
		if (m_rules->showHealthBar) m_healthBarTimer = kHealthBarSeconds;
		if (m_ai->is_dead())
		{
			if (death == DeathKind::Burst)
			{
				m_burst = true;
			}
			else if (death == DeathKind::Fling)
			{
				m_flung = true;
				b2Vec2 away = actor().position - from;
				if (away.Normalize() < 0.0001f) away = -m_ai->get_facing();
				m_flingVelocity = kFlingSpeed * away;
				m_liftSpeed = kFlingLift;
			}
			on_death(game);
			// A tourist shot down before it began its attack blows up at once (HealthComponent: EXPLODE_BEGIN instead of
			// dying); once it began, its attack goes on to the end
			if (m_rules->attack == EnemyAttack::Bomber && m_phase == Phase::None)
			{
				explode(game);
			}
			return amount;
		}
		game.play(m_rules->hurtSound, actor().position);
		// An attacking tourist cannot be pushed away (EnemyComponent)
		if (knockback && !(m_rules->attack == EnemyAttack::Bomber && m_phase != Phase::None))
		{
			// EnemyComponent: knocked back from the player, 2000/mass cm/s, for 2000/mass ms (heavy ones not at all)
			const float seconds = 2.0f / m_rules->mass;
			if (seconds >= 0.1f)
			{
				m_ai->knock_back(from, 20.0f / m_rules->mass, seconds);
				game.play(game.rules().sound("meleeHit"), actor().position);
			}
		}
		else if (m_hurtClipTimer <= 0.0f)
		{
			// HealthComponent: the hurt clip plays at most once every 2 s, and not during an attack; while it plays no attack
			// starts (EnemyComponent ES_HURT)
			m_hurtClipTimer = kHurtClipInterval;
			if (!m_ai->is_attacking())
			{
				++m_hurtClips;
				if (m_rules->hurtClipMs > 0.0f) m_ai->hold_attacks(m_rules->hurtClipMs / 1000.0f);
			}
		}
		return amount;
	}

	bool EnemyBrainComponent::knocked_back() const { return m_ai->is_knocked_back(); }
	float EnemyBrainComponent::knock_seconds_left() const { return m_ai->get_knock_seconds_left(); }

	float EnemyBrainComponent::get_attack_duration() const
	{
		const EnemyConfig& config = m_ai->get_config();
		return std::max(config.attackDuration, config.attackWindup);
	}

	void EnemyBrainComponent::teleport(const b2Vec2& at) { m_ai->teleport(at); }

	void EnemyBrainComponent::kill(ZombiesGame& game)
	{
		damage(game, DamageInfo{ .amount = 1.0e9f });
	}

	void EnemyBrainComponent::on_death(ZombiesGame& game)
	{
		if (m_deathHandled)
		{
			return;
		}
		m_deathHandled = true;
		game.enemy_killed(actor());
		game.play(m_rules->deathSound, actor().position);
		game.roll_drop(m_rules->randomDrop, m_rules->drops, m_rules->dropRate, actor().position);
		// what it took from the floor comes back, always
		for (size_t i = 0; i < m_carried.size(); ++i)
		{
			const float angle = static_cast<float>(i) * 2.4f;
			game.drop_pickup(m_carried[i], actor().position + 0.5f * static_cast<float>(i > 0) * b2Vec2(std::cos(angle), std::sin(angle)));
		}
		m_carried.clear();
		m_coin = 0;
	}

	bool EnemyBrainComponent::free_for_coin() const
	{
		return !m_ai->is_dead() && !special_attack() && !m_ai->is_attacking() && !m_ai->is_knocked_back() && m_coin == 0 && m_fleeSeconds <= 0.0f;
	}

	void EnemyBrainComponent::choose_victim(ZombiesGame& game, Actor* player, b2Vec2& target, Actor*& victim)
	{
		if (special_attack())
		{
			return;   // the special ones keep to the player
		}
		const b2Vec2 at = actor().position;
		// The survivor escorted: whichever of the two is nearer
		if (Actor* ally = game.escorted ? game.find(game.escorted) : nullptr)
		{
			const HealthComponent* health = ally->get<HealthComponent>();
			if (health && !health->is_dead() && (ally->position - at).Length() < (target - at).Length())
			{
				victim = ally;
				target = ally->position;
			}
		}
		// The thing to defend: two of every three zombies go for it, unless the player is right next to them
		if (Actor* goal = game.objective ? game.find(game.objective) : nullptr)
		{
			const DestructibleComponent* prop = goal->get<DestructibleComponent>();
			const bool playerNear = player && (player->position - at).Length() < kObjectiveIgnoreNear;
			if (prop && !prop->is_broken() && actor().id % 3 != 0 && !playerNear)
			{
				victim = goal;
				target = prop->reach_point(at, 0.35f);
			}
		}
		// A barricade standing between it and where it goes: it breaks it first
		for (uintptr_t id : game.barricades)
		{
			Actor* barricade = game.find(id);
			const DestructibleComponent* prop = barricade ? barricade->get<DestructibleComponent>() : nullptr;
			if (!prop || prop->is_broken())
			{
				continue;
			}
			const b2Vec2 point = prop->reach_point(at, 0.35f);
			if ((point - at).Length() < kBarricadeNear && b2Dot(target - at, prop->reach_point(at, 0.0f) - at) > 0.0f)
			{
				victim = barricade;
				target = point;
				return;
			}
		}
	}

	bool EnemyBrainComponent::sees(ZombiesGame& game, const b2Vec2& at) const
	{
		return (at - actor().position).Length() <= m_ai->get_config().sightRange && m_ai->has_line_of_sight(game.world(), at);
	}

	void EnemyBrainComponent::go_for_coin(uintptr_t coin)
	{
		m_coin = coin;
		m_coinSeconds = 0.0f;
	}

	float EnemyBrainComponent::health_share() const
	{
		return m_ai->health.maximum > 0.0f ? std::max(0.0f, m_ai->health.current / m_ai->health.maximum) : 0.0f;
	}

	CharacterViewState EnemyBrainComponent::view_state() const
	{
		CharacterViewState state;
		state.dead = is_dead();
		state.attacking = is_attacking();
		state.attackDirection = attack_direction();
		state.attackSeconds = get_attack_duration();
		state.hurt = hurt_recently();
		state.hurtClips = m_hurtClips;
		state.knocked = knocked_back();
		state.knockLeft = knock_seconds_left();
		state.burst = m_burst;
		state.lift = m_lift;
		state.tumble = m_tumble;
		// Fading away: after the death clip the body sinks into the floor
		if (state.dead && m_rules->fadeOut && m_deadSeconds > 1.0f)
		{
			state.sink = std::min(1.0f, (m_deadSeconds - 1.0f) / std::max(0.001f, m_rules->fadeOutMs / 1000.0f));
		}
		state.enraged = m_berserk;
		state.special = special_attack();
		state.action = &m_action;
		state.facing = facing();
		return state;
	}

	void EnemyBrainComponent::update(TopDownGame& topDown, float dt)
	{
		ZombiesGame& game = zombies(topDown);
		m_hurtTimer = std::max(0.0f, m_hurtTimer - dt);
		m_hurtClipTimer = std::max(0.0f, m_hurtClipTimer - dt);
		m_healthBarTimer = m_ai->is_dead() ? 0.0f : std::max(0.0f, m_healthBarTimer - dt);
		Actor* player = game.player();
		// A tourist shot during its attack still blows up on time
		if (m_ai->is_dead() && m_rules->attack == EnemyAttack::Bomber && (m_phase == Phase::Prepare || m_phase == Phase::Explode))
		{
			update_special(game, dt, player);
		}
		if (m_ai->is_dead())
		{
			// Flung: it flies up and falls back, turning over; the time a body stays counts from its landing
			if (m_flung && (m_lift > 0.0f || m_liftSpeed > 0.0f))
			{
				actor().position += dt * m_flingVelocity;
				m_lift += m_liftSpeed * dt;
				m_liftSpeed -= kFlingGravity * dt;
				m_tumble += kFlingTumble * dt;
				if (m_lift <= 0.0f)
				{
					m_lift = 0.0f;
					m_liftSpeed = 0.0f;
				}
				return;
			}
			m_deadSeconds += dt;
			// The body lies there, then fades away (Health IsFadeOut / FadeOutTime)
			float stay = m_burst ? 1.0f : (m_rules->fadeOut ? m_rules->fadeOutMs / 1000.0f + 1.0f : 1.0e9f);
			if (m_flung) stay = std::max(stay, kFlungStaySeconds);
			if (m_deadSeconds >= stay)
			{
				actor().removed = true;
			}
			return;
		}
		b2Vec2 target = player ? player->position : actor().position;
		Actor* victim = player;   // who its swings hurt
		if (m_coin == 0 && m_fleeSeconds <= 0.0f)
		{
			choose_victim(game, player, target, victim);
		}
		float coinSpeed = 0.0f;   // > 0: going for a coin or running off with it, at this share of its speed
		if (m_coin != 0)
		{
			Actor* coin = game.find(m_coin);
			m_coinSeconds += dt;
			if (!coin || coin->removed || m_coinSeconds > kCoinGiveUpSeconds)
			{
				m_coin = 0;   // the player took it first, or it could not get there
			}
			else if ((coin->position - actor().position).Length() <= kCoinReach)
			{
				m_carried.push_back(coin->gameId);
				coin->removed = true;
				if (const PickupComponent* pickup = coin->get<PickupComponent>()) game.play(pickup->rules().sound, coin->position);
				m_coin = 0;
				m_fleeSeconds = kCoinFleeSeconds;
			}
			else
			{
				target = coin->position;
				coinSpeed = 1.0f;
			}
		}
		if (m_fleeSeconds > 0.0f && player)
		{
			// away from the player: straight back, or the nearest turn of that with floor to run on
			m_fleeSeconds -= dt;
			b2Vec2 away = actor().position - player->position;
			if (away.Normalize() < 0.001f) away = b2Vec2(1.0f, 0.0f);
			const float base = std::atan2(away.y, away.x);
			for (float turn : { 0.0f, 0.6f, -0.6f, 1.2f, -1.2f, 1.8f, -1.8f })
			{
				const b2Vec2 way(std::cos(base + turn), std::sin(base + turn));
				if (game.nav().is_walkable(actor().position + 2.0f * way))
				{
					target = actor().position + 5.0f * way;
					break;
				}
			}
			coinSpeed = kCoinFleeSpeed;
		}
		if (coinSpeed > 0.0f) m_ai->hold_attacks(0.2f);   // busy with the coin: it does not swing
		// The game reads the tables at the current difficulty, so a change reaches the zombies already out (keeping their
		// share of health)
		if (game.difficulty != m_difficulty)
		{
			m_difficulty = game.difficulty;
			m_ai->set_config(config_for(*m_rules, m_berserk, m_difficulty, m_layers), true);
		}
		// Slower near the player (SlowDownDistance 10 m, SlowDownSpeed 0.8), faster far off screen (MoveSpeedOffScreen)
		float speedScale = (target - actor().position).Length() < kSlowDownDistance ? kSlowDownSpeed : 1.0f;
		if (m_rules->offscreenSpeed > 0.0f && game.far_off_screen(actor().position))
		{
			speedScale = meters(m_rules->offscreenSpeed) / std::max(0.01f, m_ai->get_config().speed);
		}
		if (coinSpeed > 0.0f) speedScale = coinSpeed;
		if (bear_still())
		{
			speedScale = 0.0f;
			m_ai->hold_attacks(0.2f);
		}
		m_ai->set_speed_scale(speedScale);
		const float hit = m_ai->update(dt, target, game.nav(), game.world());
		actor().position = m_ai->get_position();
		if (m_ai->attack_started())
		{
			game.play(m_rules->attackSound, actor().position);
		}
		if (special_attack())
		{
			update_special(game, dt, player);
			return;
		}
		if (hit > 0.0f && victim && victim != player)
		{
			game.damage(*victim, { .amount = hit, .type = DamageType::Attack, .source = actor().id });   // a barricade, the thing to defend, the one escorted
		}
		else if (hit > 0.0f && player)
		{
			game.damage(*player, { .amount = hit, .type = DamageType::Attack, .source = actor().id });
			// A paparazzi's flash also slows the player down (FlashAttack OtherDamage for AttackTime)
			if (m_rules->flash && m_rules->slowMs > 0.0f)
			{
				if (PlayerControlComponent* control = player->get<PlayerControlComponent>())
				{
					control->slow_down(m_rules->slowFactor, m_rules->slowMs / 1000.0f);
					game.play(game.rules().sound("playerSlowed"), player->position);
				}
			}
		}
	}

	void EnemyBrainComponent::show(const std::string& name, float seconds, bool loop)
	{
		m_action.name = name;
		m_action.seconds = seconds;
		m_action.loop = loop;
		++m_action.serial;
	}

	void EnemyBrainComponent::end_special()
	{
		m_phase = Phase::None;
		m_phaseSeconds = 0.0f;
		m_action.name.clear();
		m_ai->end_attack();
	}

	// BomberAttackComponent::Explode: everything alive within AttackRange takes the enemy's damage, the player and other
	// zombies too (blasted tourists go off in turn); nothing is left of the tourist
	void EnemyBrainComponent::explode(ZombiesGame& game)
	{
		if (m_exploded)
		{
			return;
		}
		m_exploded = true;
		m_burst = true;
		m_action.name.clear();
		const b2Vec2 at = actor().position;
		const float radius = meters(m_rules->attackRange);
		game.events.emit(ExplosionEvent{ at, radius, game.rules().shake.enemyBlast });
		game.enemy_blast(at, radius, m_ai->get_config().attackDamage, actor().id);
	}

	void EnemyBrainComponent::update_special(ZombiesGame& game, float dt, Actor* player)
	{
		const b2Vec2 position = actor().position;
		const b2Vec2 target = player ? player->position : position;
		const float distance = (target - position).Length();
		const float damage = m_ai->get_config().attackDamage;
		const bool started = m_ai->attack_started();
		// Pushed back in the middle: the attack is over (a tourist shot down goes on)
		if (!started && m_phase != Phase::None && !bear_still() && !m_ai->is_attacking() && !(m_rules->attack == EnemyAttack::Bomber && m_ai->is_dead()))
		{
			m_phase = Phase::None;
			m_action.name.clear();
		}
		m_phaseSeconds += dt;
		auto enter = [this](Phase phase) { m_phase = phase; m_phaseSeconds = 0.0f; };

		switch (m_rules->attack)
		{
		case EnemyAttack::Boomer:
			// BoomerAttack: it stands and plays its clip; DamageDelay into it the blast hurts the player within AttackRange,
			// and four shots leave in a cross (ahead, behind and to both sides of where it faces)
			if (started)
			{
				enter(Phase::Windup);
				show("attack", m_rules->attackClipMs / 1000.0f, false);
			}
			if (m_phase == Phase::Windup && m_phaseSeconds >= m_rules->attackDelayMs / 1000.0f)
			{
				m_phase = Phase::Recover; // the clock goes on: the attack ends with the clip
				// The original shows a flat flash on the ground (its AttackEffectMesh), not an explosion: a ring of dust here
				game.events.emit(ShockwaveEvent{ position, meters(m_rules->attackRange), game.rules().shake.enemyBlast });
				if (player && distance < meters(m_rules->attackRange))
				{
					game.damage(*player, { .amount = damage, .type = DamageType::Attack, .source = actor().id });
				}
				const b2Vec2 ahead = m_ai->get_attack_direction();
				for (const b2Vec2& way : { ahead, -ahead, b2Vec2(-ahead.y, ahead.x), b2Vec2(ahead.y, -ahead.x) })
				{
					game.fire_enemy_bullet(position + kShotStart * way, way, meters(m_rules->shotSpeed), m_rules->shotDamage, meters(m_rules->shotRangeCm));
				}
			}
			if (m_phase == Phase::Recover && m_phaseSeconds >= std::max(m_rules->attackClipMs, m_rules->attackDelayMs) / 1000.0f)
			{
				end_special();
			}
			break;

		case EnemyAttack::Bomber:
			// BomberAttack: PrepareTime of the prepare clip, then the explode clip; DamageDelay into it, it blows up. (The
			// original also ends it when the explode clip ends, a little before the delay; here the blast always goes off.)
			// Unlike the original (it stands where it started, AttackRadius 4 m away, and its 3 m blast mostly missed), it keeps
			// running at the player until it is a meter away, so the blast lands on the player.
			if (started)
			{
				enter(Phase::Prepare);
				show("prepare", 0.0f, true);
			}
			if ((m_phase == Phase::Prepare || m_phase == Phase::Explode) && !m_ai->is_dead())
			{
				m_ai->set_attack_move(distance > kBomberCloseIn ? m_ai->get_config().speed : 0.0f, true);
			}
			if (m_phase == Phase::Prepare && m_phaseSeconds >= m_rules->prepareMs / 1000.0f)
			{
				enter(Phase::Explode);
				show("explode", std::max(m_rules->explodeClipMs, m_rules->attackDelayMs) / 1000.0f, false);
			}
			if (m_phase == Phase::Explode && m_phaseSeconds >= m_rules->attackDelayMs / 1000.0f)
			{
				m_phase = Phase::None;
				if (!m_ai->is_dead())
				{
					// it dies of its own blast: no reward
					m_deathHandled = true;
					m_ai->apply_damage(1.0e9f);
				}
				explode(game);
			}
			break;

		case EnemyAttack::DogWalker:
		{
			// DogWalkerAttack: within AttackRadius it backs off from a player closer than BackOffRadius, and otherwise lets a
			// dog loose every SpawnInterval while fewer than SpawnActive of its dogs are alive; the dog comes out SpawnDelay
			// into the release clip. It gives up when the player is beyond AttackRadius.
			m_dogs.erase(std::remove_if(m_dogs.begin(), m_dogs.end(), [&game](uintptr_t id)
			{
				const Actor* dog = game.find(id);
				const EnemyBrainComponent* brain = dog && !dog->removed ? dog->get<EnemyBrainComponent>() : nullptr;
				return !brain || brain->is_dead();
			}), m_dogs.end());
			if (started)
			{
				enter(Phase::Run);
			}
			if (m_phase == Phase::Releasing)
			{
				m_ai->set_attack_move(0.0f, false); // it does not turn while it lets a dog out
				if (!m_dogOut && m_phaseSeconds >= m_rules->spawnDelayMs / 1000.0f)
				{
					m_dogOut = true;
					const uintptr_t dog = game.spawnEnemy ? game.spawnEnemy(m_rules->spawnType, position + kShotStart * m_ai->get_facing()) : 0;
					if (dog != 0) m_dogs.push_back(dog);
				}
				if (m_phaseSeconds >= m_rules->spawnClipMs / 1000.0f)
				{
					enter(Phase::Run);
					m_action.name.clear();
					if (distance > meters(m_rules->attackRadius)) end_special();
				}
				break;
			}
			if (m_phase != Phase::Run)
			{
				break;
			}
			if (distance > meters(m_rules->attackRadius))
			{
				end_special();
			}
			else if (distance < meters(m_rules->backOffRadius))
			{
				m_ai->set_attack_move(-meters(m_rules->backwardSpeed), true);
				if (m_action.name != "backOff") show("backOff", 0.0f, true);
			}
			else
			{
				m_ai->set_attack_move(0.0f, true);
				if (m_action.name == "backOff") m_action.name.clear();
				const bool more = m_rules->spawnTotal < 0 || m_dogsReleased < m_rules->spawnTotal;
				if (game.timeMs - m_lastReleaseMs > m_rules->spawnIntervalMs && static_cast<int>(m_dogs.size()) < m_rules->spawnActive && more)
				{
					enter(Phase::Releasing);
					m_dogOut = false;
					++m_dogsReleased;
					m_lastReleaseMs = game.timeMs;
					show("release", m_rules->spawnClipMs / 1000.0f, false);
				}
			}
			break;
		}

		case EnemyAttack::Dog:
			// DogAttack: it appears (its spawn clip), runs at the player at RunSpeed, pounces within PounceRadius and flies on;
			// touching the player at any point it bites once and is gone (the latching bite of the original is compiled out)
			if (started)
			{
				enter(Phase::Spawn);
				show("spawn", m_rules->spawnClipMs / 1000.0f, false);
			}
			if (m_phase == Phase::None)
			{
				break;
			}
			if (player && distance < meters(m_rules->collisionSize) * 0.5f + kPlayerBodyRadius)
			{
				game.damage(*player, { .amount = damage, .type = DamageType::Attack, .source = actor().id });
				m_deathHandled = true; // no reward
				m_burst = true;
				m_phase = Phase::None;
				m_action.name.clear();
				m_ai->apply_damage(1.0e9f);
				break;
			}
			if (m_phase == Phase::Spawn && m_phaseSeconds >= m_rules->spawnClipMs / 1000.0f)
			{
				enter(Phase::Run);
				show("run", 0.0f, true);
			}
			if (m_phase == Phase::Run && distance < meters(m_rules->pounceRadius))
			{
				enter(Phase::Pounce);
				show("pounce", m_rules->pounceClipMs / 1000.0f, false);
			}
			if (m_phase == Phase::Pounce && m_phaseSeconds >= m_rules->pounceClipMs / 1000.0f)
			{
				enter(Phase::InAir);
				show("inAir", 0.0f, true);
			}
			m_ai->set_attack_move(m_phase == Phase::Spawn ? 0.0f : meters(m_rules->runSpeed), true);
			break;

		case EnemyAttack::Bear:
		{
			// ZombieBear: asleep, then waking up; on all fours it swats, standing it swings its chain (ZombieBearComponent)
			const float health = m_ai->health.current;
			if (m_phase == Phase::Sleep)
			{
				if (m_phaseSeconds >= m_rules->sleepMs / 1000.0f)
				{
					enter(Phase::Wake);
					show("wake", m_rules->wakeClipMs / 1000.0f, false);
					game.play(game.rules().sound("bearWake"), position);
				}
				break;
			}
			if (m_phase == Phase::Wake || m_phase == Phase::Rear)
			{
				if (m_phaseSeconds >= std::max(0.3f, (m_phase == Phase::Wake ? m_rules->wakeClipMs : m_rules->rearClipMs) / 1000.0f))
				{
					m_phase = Phase::None;
					m_action.name.clear();
				}
				break;
			}
			if (m_wokeHealth <= 0.0f) m_wokeHealth = health;
			if (!m_reared && m_phase == Phase::None && !m_ai->is_attacking()
				&& (m_wokeHealth - health) / std::max(1.0f, m_ai->health.maximum) >= m_rules->rearShare)
			{
				m_reared = true;
				enter(Phase::Rear);
				show("rear", m_rules->rearClipMs / 1000.0f, false);
				m_ai->set_config(reared_config(config_for(*m_rules, m_berserk, m_difficulty, m_layers), *m_rules), true);
				game.play(game.rules().sound("bearRear"), position);
				break;
			}
			const float delay = (m_reared ? m_rules->chainDelayMs : m_rules->attackDelayMs) / 1000.0f;
			const float clip = std::max(delay, (m_reared ? m_rules->chainClipMs : m_rules->attackClipMs) / 1000.0f);
			if (started)
			{
				enter(Phase::Windup);
				show(m_reared ? "chain" : "swat", clip, false);
			}
			if (m_phase == Phase::Windup && m_phaseSeconds >= delay)
			{
				m_phase = Phase::Recover;
				const float reach = meters(m_reared ? m_rules->chainRange : m_rules->attackRange);
				if (player && distance < reach) game.damage(*player, { .amount = damage, .type = DamageType::Attack, .source = actor().id });
				if (m_reared) game.events.emit(ShakeEvent{ position, game.rules().shake.explosion });
			}
			if (m_phase == Phase::Recover && m_phaseSeconds >= clip)
			{
				end_special();
			}
			break;
		}

		case EnemyAttack::Constant:
			// ConstantAttack: it windmills its arms and keeps walking at RunSpeed; the player within AttackRange takes its
			// damage per second (as in the original, a hit makes the player invincible for a moment, so much of it is lost)
			if (started)
			{
				enter(Phase::Windmill);
				show("windmill", m_rules->attackClipMs / 1000.0f, true);
				m_ai->set_attack_move(meters(m_rules->runSpeed), true);
			}
			if (m_phase != Phase::Windmill)
			{
				break;
			}
			if (distance > meters(m_rules->attackRadius))
			{
				end_special();
			}
			else if (player && distance < meters(m_rules->attackRange))
			{
				game.damage(*player, { .amount = damage * dt, .type = DamageType::Attack, .source = actor().id });
			}
			break;

		default:
			break;
		}
	}
}
