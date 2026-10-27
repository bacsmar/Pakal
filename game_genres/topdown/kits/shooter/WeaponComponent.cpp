///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: WeaponComponent.cpp
// Purpose: The Shooter kit's weapon - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/kits/shooter/WeaponComponent.h"
#include "topdown/Height.h"
#include "topdown/TopDownGame.h"

#include <algorithm>
#include <cmath>

namespace Pakal::TopDown
{
	namespace
	{
		// The trapezoid a melee weapon hits (MeleeWeapon): half widths at the player and at the reach (meters)
		const float kMeleeNearHalfWidth = 0.1f;
		const float kMeleeFarHalfWidth = 2.0f;
		const float kFireClipSeconds = 0.35f;
		const float kSwingClipSeconds = 0.3f;
		const float kMuzzleForward = 0.4f;       // where bullets start, in front of the player
		const float kMuzzleSide = 0.15f;         // half the distance between the two guns
		const float kSwingHeight = 1.8f;         // a swing reaches this high from the feet (Height.h)
		const float kMuzzleHeight = 1.0f;        // bullets fly this high over the shooter's feet

		// Whether a circle at (x, y) touches the trapezoid from (-near, 0)-(near, 0) to (-far, reach)-(far, reach)
		bool circle_touches_trapezoid(float x, float y, float radius, float nearHalf, float farHalf, float reach)
		{
			const b2Vec2 corners[4] = { b2Vec2(-nearHalf, 0.0f), b2Vec2(nearHalf, 0.0f), b2Vec2(farHalf, reach), b2Vec2(-farHalf, reach) };
			const b2Vec2 point(x, y);
			bool inside = true;
			float nearest = 1.0e9f;
			for (int i = 0; i < 4; ++i)
			{
				const b2Vec2 a = corners[i];
				const b2Vec2 edge = corners[(i + 1) % 4] - a;
				// the corners go counterclockwise: the inside is to the left of each edge
				if (b2Cross(edge, point - a) < 0.0f) inside = false;
				const float t = std::clamp(b2Dot(point - a, edge) / edge.LengthSquared(), 0.0f, 1.0f);
				nearest = std::min(nearest, (point - (a + t * edge)).Length());
			}
			return inside || nearest <= radius;
		}
	}

	WeaponComponent::WeaponComponent(const GunRules& gun) :
		m_gun(gun),
		m_magazine(gun.magazine),
		m_rounds(std::max(0, gun.rounds - gun.magazine))
	{
		m_reload.onPhase = [this](ActionPhase phase)
		{
			if (phase != ActionPhase::Active) return;
			int& magazine = m_firearm ? m_fireMagazine : m_magazine;
			int& rounds = m_firearm ? m_fireRounds : m_rounds;
			const int loaded = std::min(this->gun().magazine - magazine, rounds);
			magazine += loaded;
			rounds -= loaded;
		};
	}

	float WeaponComponent::run_speed() const
	{
		const ConsumableComponent* consumable = actor().get<ConsumableComponent>();
		const MeleeRules* melee = consumable ? consumable->melee() : nullptr;
		const GunRules& g = gun();
		const float speed = melee ? melee->runSpeed : (m_fireClipTimer > 0.0f ? g.fireSpeed : g.runSpeed);
		return meters(speed) * stat_of(actor(), Stat::speed);
	}

	void WeaponComponent::update(TopDownGame& game, float dt)
	{
		m_melee.update(dt);
		m_fireClipTimer = std::max(0.0f, m_fireClipTimer - dt);
		m_swingTimer = std::max(0.0f, m_swingTimer - dt);
		m_recoil = std::max(0.0f, m_recoil - dt * 9.0f);

		// A firearm the player has picked up replaces the gun (with its own rounds) until its timer ends
		const ConsumableComponent* consumable = actor().get<ConsumableComponent>();
		const GunRules* lent = consumable ? consumable->firearm() : nullptr;
		if (lent != m_firearm)
		{
			m_firearm = lent;
			m_reload.cancel();
			if (lent)
			{
				m_fireMagazine = lent->magazine;
				m_fireRounds = std::max(0, lent->rounds - lent->magazine);
			}
		}
		const GunRules& g = gun();
		int& magazine = m_firearm ? m_fireMagazine : m_magazine;
		int& rounds = m_firearm ? m_fireRounds : m_rounds;
		m_reload.update(dt);

		const float damageFactor = stat_of(actor(), Stat::damage);
		if (const MeleeRules* melee = consumable ? consumable->melee() : nullptr)
		{
			m_cycle = -1.0f;
			// no windup: it hits as the swing starts
			if (m_trigger && m_melee.start({ .cooldown = melee->attackRateMs / 1000.0f }))
			{
				swing(game, *melee, damageFactor);
			}
			return;
		}
		if (m_reload.busy())
		{
			m_cycle = -1.0f;
			return;
		}
		// The gun fires in cycles of FireRate (its fire clip): each cycle lets out a shot at each trigger time. Holding the
		// trigger chains the cycles; letting go stops the shots still to come (the first one of a cycle always leaves).
		const float cycleSeconds = std::max(0.01f, g.fireRateMs / 1000.0f);
		if (m_cycle < 0.0f)
		{
			if (!m_trigger || magazine <= 0)
			{
				return;
			}
			m_cycle = 0.0f;
			m_nextTrigger = 0;
		}
		else
		{
			m_cycle += dt;
		}
		while (m_cycle >= 0.0f)
		{
			while (m_nextTrigger < g.triggers.size() && m_cycle >= g.triggers[m_nextTrigger] * cycleSeconds)
			{
				if ((m_nextTrigger == 0 || m_trigger) && magazine > 0)
				{
					fire_gun(game, damageFactor);
					m_fireClipTimer = cycleSeconds;
				}
				++m_nextTrigger;
			}
			if (m_cycle < cycleSeconds)
			{
				break;
			}
			m_cycle -= cycleSeconds;
			m_nextTrigger = 0;
			if (!m_trigger || magazine <= 0)
			{
				m_cycle = -1.0f;
			}
		}
		if (magazine == 0 && rounds > 0)
		{
			m_cycle = -1.0f;
			m_reload.start({ .windup = g.reloadMs / 1000.0f });
			game.play(g.reloadSound, actor().position);
		}
	}

	void WeaponComponent::fire_gun(TopDownGame& game, float damageFactor)
	{
		const PlayerControlComponent* control = actor().get<PlayerControlComponent>();
		const b2Vec2 facing = control ? control->facing() : b2Vec2(0.0f, 1.0f);
		const b2Vec2 side(facing.y, -facing.x);
		const GunRules& g = gun();
		b2Vec2 from = actor().position + kMuzzleForward * facing;
		if (g.dualMuzzles)
		{
			m_leftMuzzle = !m_leftMuzzle;
			from += (m_leftMuzzle ? -kMuzzleSide : kMuzzleSide) * side;
		}
		for (int pellet = 0; pellet < g.pellets; ++pellet)
		{
			// the pellets of one shot spread evenly from one side of the angle to the other
			b2Vec2 direction = facing;
			if (g.pellets > 1 && g.spreadDegrees > 0.0f)
			{
				const float t = static_cast<float>(pellet) / static_cast<float>(g.pellets - 1) - 0.5f;
				const float angle = t * g.spreadDegrees * 0.0174532925f;
				direction = b2Vec2(facing.x * std::cos(angle) - facing.y * std::sin(angle), facing.x * std::sin(angle) + facing.y * std::cos(angle));
			}
			game.fire_bullet(from, direction, meters(g.bulletSpeed), g.bulletDamage * damageFactor, meters(g.rangeCm), g.piercing, g.knockback,
				game.faction_of(actor()), height_of(actor()) + kMuzzleHeight);
		}
		game.play(g.fireSound, actor().position);
		game.events.emit(ShotEvent{ from, facing });
		if (m_firearm) --m_fireMagazine; else --m_magazine;
		m_fireClipTimer = std::max(m_fireClipTimer, kFireClipSeconds);
		m_recoil = 1.0f;
	}

	void WeaponComponent::swing(TopDownGame& game, const MeleeRules& melee, float damageFactor)
	{
		const PlayerControlComponent* control = actor().get<PlayerControlComponent>();
		const b2Vec2 facing = control ? control->facing() : b2Vec2(0.0f, 1.0f);
		const float reach = meters(melee.reach);
		game.play(melee.swingSound, actor().position);
		m_swingTimer = kSwingClipSeconds;
		for (const auto& other : game.actors())
		{
			// it hits the characters hostile to the one swinging, and props
			if (other.get() == &actor() || other->removed || !(other->kind == ActorKind::Prop || (other->kind == ActorKind::Character && game.hostile(actor(), *other)))
				|| !heights_meet(actor(), *other, kSwingHeight))
			{
				continue;
			}
			const float radius = game.hit_radius(*other);
			const b2Vec2 to = other->position - actor().position;
			bool hit = false;
			if (melee.circle)
			{
				hit = to.Length() <= reach + radius;
			}
			else
			{
				// in the weapon's frame: y ahead, x to the side
				const b2Vec2 side(facing.y, -facing.x);
				hit = circle_touches_trapezoid(b2Dot(to, side), b2Dot(to, facing), radius, kMeleeNearHalfWidth, kMeleeFarHalfWidth, reach);
			}
			if (hit)
			{
				// melee weapons knock back
				game.damage(*other, { .amount = melee.damage * damageFactor, .type = DamageType::Melee, .source = actor().id, .from = actor().position, .knockback = true, .death = melee.death });
			}
		}
	}
}
