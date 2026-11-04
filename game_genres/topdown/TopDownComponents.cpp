///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: TopDownComponents.cpp
// Purpose: The components any top-down action game shares - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/TopDownComponents.h"
#include "topdown/ActorOrientation.h"
#include "topdown/TopDownGame.h"

#include "box2D/CharacterController2D.h"
#include "box2D/PhysicsWorld2D.h"
#include "gameplay/NavGrid2D.h"
#include "LogMgr.h"

#include <algorithm>
#include <cmath>

namespace Pakal::TopDown
{
	namespace
	{
	}

	// Component ------------------------------------------------------------------------------------

	void Component::update(Pakal::SimWorld& world, float dt)
	{
		update(static_cast<TopDownGame&>(world), dt);
	}

	// HealthComponent ------------------------------------------------------------------------------

	HealthComponent::HealthComponent(float maximum, float invincibleSeconds) : m_invincibleSeconds(invincibleSeconds)
	{
		m_health.maximum = m_health.current = maximum;
	}

	float HealthComponent::damage(float amount)
	{
		if (is_dead() || m_invincibleTimer > 0.0f || amount <= 0.0f)
		{
			return 0.0f;
		}
		const float taken = amount;
		m_health.apply_damage(taken);
		m_invincibleTimer = m_invincibleSeconds;
		m_hurtTimer = hurtFlashSeconds;
		return taken;
	}

	void HealthComponent::heal(float amount)
	{
		if (!is_dead())
		{
			m_health.current = std::min(m_health.maximum, m_health.current + amount);
		}
	}

	void HealthComponent::update(TopDownGame& game, float dt)
	{
		(void)game;
		m_invincibleTimer = std::max(0.0f, m_invincibleTimer - dt);
		m_hurtTimer = std::max(0.0f, m_hurtTimer - dt);
	}

	// CharacterBodyComponent --------------------------------------------------------------------------

	CharacterBodyComponent::CharacterBodyComponent(TopDownGame& game, const b2Vec2& at, float radius, uint16_t layer)
	{
		// The id is set when the component is added; the body takes it on its first update
		m_body = std::make_unique<Pakal::CharacterController2D>(game.world().native(), at, radius, layer != 0 ? layer : game.layers.player, 0xFFFF, 0);
	}

	CharacterBodyComponent::~CharacterBodyComponent() = default;

	void CharacterBodyComponent::move(const b2Vec2& direction, float speed)
	{
		m_body->set_move(direction, speed);
	}

	void CharacterBodyComponent::teleport(const b2Vec2& at)
	{
		m_body->teleport(at);
		actor().position = at;
	}

	void CharacterBodyComponent::set_enabled(bool enabled)
	{
		m_body->set_enabled(enabled);
	}

	void CharacterBodyComponent::update(TopDownGame& game, float dt)
	{
		(void)game;
		(void)dt;
		m_body->update();
		actor().position = m_body->get_position();
	}

	// EscortComponent ------------------------------------------------------------------------------

	void EscortComponent::update(TopDownGame& game, float dt)
	{
		CharacterBodyComponent* body = actor().get<CharacterBodyComponent>();
		const HealthComponent* health = actor().get<HealthComponent>();
		if (m_state == State::Dead || m_state == State::Arrived || !body)
		{
			if (body) body->move(b2Vec2(0.0f, 0.0f), 0.0f);
			return;
		}
		if (health && health->is_dead())
		{
			m_state = State::Dead;
			body->move(b2Vec2(0.0f, 0.0f), 0.0f);
			return;
		}
		const b2Vec2 at = actor().position;
		// the last point (home) is reached a little farther out: it may sit where the walking grid's body does not fit
		while (m_next < m_route.size() && (m_route[m_next] - at).Length() < (m_next + 1 == m_route.size() ? 3.0f : pointReach)) ++m_next;
		if (m_next >= m_route.size())
		{
			m_state = State::Arrived;
			body->move(b2Vec2(0.0f, 0.0f), 0.0f);
			return;
		}
		// it goes with the players: it waits while the nearest one is far (unless it walks on its own), or while held
		Actor* player = waitsForPlayer ? game.nearest_player(at, false) : nullptr;
		if (held || (waitsForPlayer && (!player || (player->position - at).Length() > waitDistance)))
		{
			m_state = State::Waiting;
			body->move(b2Vec2(0.0f, 0.0f), 0.0f);
			return;
		}
		m_state = State::Walking;
		m_pathTimer -= dt;
		if (m_pathTimer <= 0.0f || (m_path.empty() && !m_noRoute))
		{
			m_noRoute = !game.nav().find_path(at, m_route[m_next], m_path);
			if (m_noRoute) m_path.clear();
			m_pathTimer = 0.5f;
		}
		while (!m_path.empty() && (m_path.front() - at).Length() < 0.4f) m_path.erase(m_path.begin());
		b2Vec2 way = (m_path.empty() ? m_route[m_next] : m_path.front()) - at;
		if (way.Normalize() < 1.0e-4f) way = b2Vec2(0.0f, 0.0f);
		body->move(way, speed);
	}

	// ConsumableComponent --------------------------------------------------------------------------

	void ConsumableComponent::apply(TopDownGame& game, const ConsumableRules& rules)
	{
		end();
		game.events.emit(ConsumableEvent{ actor().id, actor().position, rules.id, rules.timerMs / 1000.0f, rules.melee, rules.firearm,
			rules.speedFactor, rules.damageFactor, rules.defenceFactor, rules.addHealth, rules.effect });
		HealthComponent* health = actor().get<HealthComponent>();
		if (rules.addHealth > 0.0f && health)
		{
			health->heal(rules.addHealth);
			game.events.emit(ShakeEvent{ actor().position, game.rules().shake.healthPack }); // Player: the health pack shakes the camera
		}
		if (!rules.effect.empty() && game.apply_effect(rules.effect, actor())) // an effect that is not a stat (an air strike)
		{
			return;
		}
		game.play(rules.sound.empty() && rules.addHealth > 0.0f ? game.rules().sound("healthPack") : rules.sound, actor().position);
		if (rules.timerMs <= 0.0f)
		{
			if (rules.addHealth <= 0.0f)
			{
				LOG_INFO("[TopDown] consumable %s (%d) has no effect here yet", rules.name.c_str(), rules.id);
			}
			return; // instant
		}
		m_active = &rules;
		m_secondsLeft = rules.timerMs / 1000.0f;
		if (StatsComponent* stats = actor().get<StatsComponent>())
		{
			m_modifiers.push_back(stats->stats.add_modifier(Stat::speed, 0.0f, rules.speedFactor));
			m_modifiers.push_back(stats->stats.add_modifier(Stat::damage, 0.0f, rules.damageFactor));
			m_modifiers.push_back(stats->stats.add_modifier(Stat::defence, 0.0f, rules.defenceFactor));
		}
	}

	void ConsumableComponent::end()
	{
		if (StatsComponent* stats = actor().get<StatsComponent>())
		{
			for (const auto id : m_modifiers) stats->stats.remove_modifier(id);
		}
		m_modifiers.clear();
		m_active = nullptr;
		m_secondsLeft = 0.0f;
	}

	void ConsumableComponent::update(TopDownGame& game, float dt)
	{
		(void)game;
		if (!m_active)
		{
			return;
		}
		m_secondsLeft -= dt;
		if (m_secondsLeft <= 0.0f)
		{
			LOG_INFO("[TopDown] %s ran out", m_active->name.c_str());
			end();
		}
	}

	// PlayerControlComponent -----------------------------------------------------------------------

	namespace
	{
		const float kAimDeadZone = 0.5f;   // meters around the player where the mouse does not turn it
	}

	void PlayerControlComponent::face(const b2Vec2& direction)
	{
		b2Vec2 way = direction;
		if (way.Normalize() < 1.0e-4f) return;
		m_facing = way;
		actor().yaw = actor_yaw(m_facing.x, -m_facing.y);
	}

	void PlayerControlComponent::slow_down(float factor, float seconds)
	{
		if (StatsComponent* stats = actor().get<StatsComponent>())
		{
			stats->stats.remove_modifier(m_slowModifier);
			m_slowModifier = stats->stats.add_modifier(Stat::speed, 0.0f, factor, seconds);
			return;
		}
		m_slowFactor = factor;
		m_slowTimer = seconds;
	}

	bool PlayerControlComponent::slowed() const
	{
		const StatsComponent* stats = actor().get<StatsComponent>();
		return stats ? stats->stats.has_modifier(m_slowModifier) : m_slowTimer > 0.0f;
	}

	void PlayerControlComponent::update(TopDownGame& game, float dt)
	{
		const PlayerInput& input = game.input_for(actor());
		CharacterBodyComponent* body = actor().get<CharacterBodyComponent>();
		PlayerWeapon* weapon = actor().get<PlayerWeapon>();
		const HealthComponent* health = actor().get<HealthComponent>();
		const bool alive = !health || !health->is_dead();
		const bool active = enabled && alive;

		// Any push of the stick walks at full speed at once, as the original (the direction is what is analog)
		b2Vec2 move = active ? input.move : b2Vec2(0.0f, 0.0f);
		m_moving = move.Normalize() > 0.0001f;
		m_slowTimer = std::max(0.0f, m_slowTimer - dt);
		if (m_slowTimer > 0.0f) move *= m_slowFactor;

		// Facing: toward the aim point (mouse), else the aim keys, else the way the player walks
		if (active)
		{
			b2Vec2 aim(0.0f, 0.0f);
			if (input.hasAimPoint) aim = input.aimPoint - actor().position;
			if (aim.LengthSquared() < kAimDeadZone * kAimDeadZone) aim = input.aimDirection; // the mouse right on the player: keep the last aim
			if (aim.LengthSquared() < 0.0001f) aim = move;
			if (aim.Normalize() > 0.0001f)
			{
				m_facing = aim;
				actor().yaw = actor_yaw(m_facing.x, -m_facing.y);
			}
		}

		if (body)
		{
			body->move(move, weapon ? weapon->run_speed() : 4.75f);
		}
		if (weapon)
		{
			weapon->set_trigger(active && input.fire);
		}
	}

	// PropComponent --------------------------------------------------------------------------------

	PropComponent::PropComponent(std::unique_ptr<PropView> view) : m_view(std::move(view))
	{
	}

	void PropComponent::apply()
	{
		if (m_view)
		{
			m_view->show_state(m_visible && m_enabled, m_broken);
		}
	}

	void PropComponent::update(TopDownGame& game, float dt)
	{
		(void)game;
		if (m_hitLeft <= 0.0f) return;
		m_hitLeft = m_broken ? 0.0f : std::max(0.0f, m_hitLeft - dt);
		m_view->show_hit(m_hitLeft / m_hitSeconds);
		if (m_hitLeft <= 0.0f) apply();   // back where it stands
	}

	// DestructibleComponent ------------------------------------------------------------------------

	DestructibleComponent::DestructibleComponent(const PropRules& rules) : m_rules(rules), m_health(rules.health)
	{
	}

	void DestructibleComponent::damage(TopDownGame& game, float amount)
	{
		if (m_broken || m_rules.health <= 0.0f)
		{
			return; // broken already, or it cannot break
		}
		game.play(game.rules().sound("propHit"), actor().position);
		const float before = m_health / m_rules.health;
		m_health -= amount;
		const float after = std::max(0.0f, m_health / m_rules.health);
		// The effects of the damage levels it passed (a window breaking, water gushing out...)
		for (const auto& level : m_rules.damageLevels)
		{
			if (level.rate < before && level.rate >= after) game.events.emit(PropEffectEvent{ actor().position, level.effect, m_size });
		}
		if (m_health > 0.0f)
		{
			if (PropComponent* prop = actor().get<PropComponent>()) prop->hit();
			return;
		}
		m_broken = true;
		if (PropComponent* prop = actor().get<PropComponent>())
		{
			prop->set_broken();
		}
		if (m_rules.repairable)
		{
			// down: enemies walk through until it is put back up
			m_health = 0.0f;
			game.world().set_static_enabled(actor().id, false);
			game.events.emit(PropEffectEvent{ actor().position, "Break", m_size });
			game.play(m_rules.deathSound, actor().position);
			return;
		}
		game.events.emit(PropEffectEvent{ actor().position, "Break", m_size });
		// gone for good (a wall of boards shot down): nothing stops at it any more, and the walking grid opens where it stood
		if (m_rules.clearWhenBroken)
		{
			game.world().set_static_enabled(actor().id, false);
			const b2Vec2 margin(1.0f, 1.0f);
			if (m_high.x > m_low.x) game.nav().rebuild_area(game.world(), m_low - margin, m_high + margin);
		}
		if (!m_rules.burningEffect.empty()) game.events.emit(PropEffectEvent{ actor().position, m_rules.burningEffect, m_size });
		game.play(m_rules.explosive ? game.rules().sound("carBlast") : m_rules.deathSound, actor().position);
		game.roll_drop(m_rules.randomDrop, m_rules.drops, m_rules.dropRate, actor().position);
		if (m_hiddenPickup != 0)
		{
			game.drop_pickup(m_hiddenPickup, actor().position);
		}
		if (m_rules.explosive && m_rules.blastRadius > 0.0f)
		{
			game.blast(actor().position, meters(m_rules.blastRadius), m_rules.blastDamage, actor().id);
		}
		LOG_INFO("[TopDown] %s %d broken", m_rules.name.c_str(), m_rules.id);
	}

	b2Vec2 DestructibleComponent::reach_point(const b2Vec2& from, float out) const
	{
		if (m_high.x <= m_low.x)
		{
			return actor().position;
		}
		const b2Vec2 nearest(std::clamp(from.x, m_low.x, m_high.x), std::clamp(from.y, m_low.y, m_high.y));
		b2Vec2 away = from - nearest;
		if (away.Normalize() < 1.0e-4f)
		{
			return nearest;   // inside its box
		}
		return nearest + out * away;
	}

	void DestructibleComponent::update(TopDownGame& game, float dt)
	{
		if (!m_rules.repairable || m_health >= m_rules.health)
		{
			return;
		}
		// A player standing next to it puts it back together; as soon as the repair starts it stops bodies again (the
		// original's RepairableBarricade)
		const auto& players = game.players();
		const bool repaired = std::any_of(players.begin(), players.end(), [&](const Actor* player)
		{
			return player && !TopDownGame::is_down(*player) && (reach_point(player->position, 0.0f) - player->position).Length() <= m_rules.repairReach;
		});
		if (!repaired)
		{
			return;
		}
		const float before = m_health;
		m_health = std::min(m_rules.health, m_health + m_rules.health / std::max(0.1f, m_rules.repairSeconds) * dt);
		if (std::floor(before / m_rules.health * 4.0f) != std::floor(m_health / m_rules.health * 4.0f))
		{
			game.play(game.rules().sound("propHit"), actor().position);   // a knock for each quarter put back
		}
		if (m_broken && m_health > 0.0f)
		{
			m_broken = false;
			game.world().set_static_enabled(actor().id, true);
			if (PropComponent* prop = actor().get<PropComponent>()) prop->set_repaired();
		}
	}

	void DestructibleComponent::restore(TopDownGame& game)
	{
		m_health = m_rules.health;
		if (m_broken)
		{
			m_broken = false;
			game.world().set_static_enabled(actor().id, true);
			if (PropComponent* prop = actor().get<PropComponent>()) prop->set_repaired();
		}
	}

	// GiftBoxComponent -----------------------------------------------------------------------------

	void GiftBoxComponent::update(TopDownGame& game, float dt)
	{
		(void)dt;
		if (!m_armed || m_opened)
		{
			return;
		}
		// The first player up and close, with no consumable in use (one in use blocks the gift: GiftBoxComponent)
		ConsumableComponent* consumable = nullptr;
		for (Actor* player : game.players())
		{
			ConsumableComponent* candidate = player ? player->get<ConsumableComponent>() : nullptr;
			if (candidate && !candidate->is_active() && !TopDownGame::is_down(*player)
				&& (player->position - actor().position).Length() <= meters(game.rules().giftRadius))
			{
				consumable = candidate;
				break;
			}
		}
		if (!consumable)
		{
			return;
		}
		m_opened = true;
		m_armed = false;
		m_given = game.rules().pick_gift(game.random01());
		auto found = game.rules().consumables.find(m_given);
		LOG_INFO("[TopDown] gift box opened: %s (%d)", found != game.rules().consumables.end() ? found->second.name.c_str() : "?", m_given);
		if (found != game.rules().consumables.end())
		{
			consumable->apply(game, found->second);
		}
		if (PropComponent* prop = actor().get<PropComponent>())
		{
			prop->show(false);
			prop->enable(false);
		}
	}

	// PickupComponent ------------------------------------------------------------------------------

	PickupComponent::PickupComponent(const PickupRules& rules, float lifeSeconds) :
		m_rules(rules),
		m_secondsLeft(lifeSeconds > 0.0f ? lifeSeconds : rules.lifeTimeMs > 0.0f ? rules.lifeTimeMs / 1000.0f : 1.0e9f)
	{
	}

	void PickupComponent::update(TopDownGame& game, float dt)
	{
		m_secondsLeft -= dt;
		if (m_secondsLeft <= 0.0f)
		{
			actor().removed = true;
			return;
		}
		m_lying += dt;
		game.pickup_lying(actor(), m_lying, dt);
		// The first player up that walks over it takes it
		for (Actor* player : game.players())
		{
			if (!player || TopDownGame::is_down(*player) || (player->position - actor().position).Length() > meters(m_rules.radius))
			{
				continue;
			}
			if (!game.take_pickup(*player, m_rules) && m_rules.consumable != 0)
			{
				auto found = game.rules().consumables.find(m_rules.consumable);
				ConsumableComponent* consumable = player->get<ConsumableComponent>();
				if (found != game.rules().consumables.end() && consumable)
				{
					consumable->apply(game, found->second);
				}
			}
			game.play(m_rules.sound, actor().position);
			game.events.emit(PickupEvent{ player->id, actor().position, actor().gameId, m_rules.type });
			actor().removed = true;
			return;
		}
	}
}
