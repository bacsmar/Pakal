///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: EnemyAI2D.h
//
// Purpose: Top-down enemy that chases a target it can see, following a path on a NavGrid2D, and
//          attacks when close. Idle otherwise. Its body is a CharacterController2D, so walls
//          stop it physically as well.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Platform.h"
#include "topdown/Health.h"
#include "gameplay/NavGrid2D.h"
#include "box2D/CharacterController2D.h"

#include <algorithm>
#include <memory>
#include <vector>

namespace Pakal::TopDown
{
	struct EnemyConfig
	{
		float radius = 0.3f;        // meters
		float speed = 1.5f;         // meters per second
		float sightRange = 12.0f;   // meters
		float attackRange = 1.0f;   // meters
		float attackDamage = 10.0f;
		float attackCooldown = 1.0f; // seconds
		float attackCooldownMax = 0.0f; // > attackCooldown: each wait is picked at random between the two
		float attackWindup = 0.0f;  // seconds from the start of an attack to its hit (0: the hit is immediate)
		float attackArcDegrees = 0.0f; // > 0: a wound-up attack hits only inside this cone, aimed when it starts
		// Seconds the whole attack lasts (its clip): the enemy stands until it ends, and the wait between attacks starts
		// then. 0: the attack ends with its hit (the windup).
		float attackDuration = 0.0f;
		// Center to center distance where it stops closing in (it keeps walking toward the target between attacks until
		// then). Negative: it stops at attackRange.
		float stopDistance = -1.0f;
		// Degrees per second it turns toward the target; an attack starts only once it faces it. 0: it turns at once.
		float turnDegreesPerSecond = 0.0f;
		// Center to center distance from which an attack may start. Negative: attackRange. A ranged enemy starts from
		// farther than it walks up to.
		float attackStartRange = -1.0f;
		// The game runs the attack: it lasts until end_attack(), deals no damage of its own, and moves and turns as
		// set_attack_move() says (standing still by default)
		bool scriptedAttack = false;
		float maxHealth = 100.0f;
		bool pursueTarget = false;  // explicit chase orders may navigate without direct sight
		// Its collision layers (the game's, see CollisionLayers): what its body is, and which layers block its sight. The
		// defaults are Box2D's: category 1, and every layer blocks.
		uint16 bodyCategory = 0x0001;
		uint16 sightBlockers = 0xFFFF;
	};

	enum class EnemyState
	{
		Idle,
		Chase,
		Dead
	};

	class EnemyAI2D
	{
	public:
		// 'id' identifies the enemy in ray cast hits (RayCastHit::userData). Must not be 0.
		EnemyAI2D(PhysicsWorld2D& world, const b2Vec2& position, const EnemyConfig& config, uintptr_t id);

		// Advances the enemy by dt. Returns the damage it deals to 'target' this step.
		// Call before PhysicsWorld2D::step. 'nav' must be built from the same world's static geometry.
		float update(float dt, const b2Vec2& target, const NavGrid2D& nav, PhysicsWorld2D& world);

		void apply_damage(float amount);

		// Hurt: for 'seconds' it may still move but does not start an attack (an attack in progress goes on)
		void hold_attacks(float seconds);
		// Knocked back away from 'from': it slides off at 'speed' (m/s), slowing down
		// (the deceleration grows with time), and stays stunned until 'seconds' have passed. An attack in progress is
		// cancelled.
		void knock_back(const b2Vec2& from, float speed, float seconds);
		bool is_knocked_back() const { return m_knockTimer > 0.0f; }
		float get_knock_seconds_left() const { return std::max(0.0f, m_knockTimer); }
		// The speed of the next update is multiplied by this (near the target, off screen...); 1 by default
		void set_speed_scale(float scale) { m_speedScale = scale; }
		void teleport(const b2Vec2& position);
		b2Vec2 get_facing() const { return m_facing; }

		// A scripted attack (EnemyConfig::scriptedAttack): ends it, so the wait before the next one starts
		void end_attack();
		// How a scripted attack in progress moves: toward the target along its path at 'speed' (m/s; negative backs away
		// from it, only over walkable ground), turning toward it when 'turns'. Reset to standing at each attack start.
		void set_attack_move(float speed, bool turns) { m_attackSpeed = speed; m_attackTurns = turns; }

		// Changes the rules of a live enemy. With keepHealthFraction, its health keeps the same share of
		// the new maximum; otherwise it is refilled.
		void set_config(const EnemyConfig& config, bool keepHealthFraction);
		const EnemyConfig& get_config() const { return m_config; }

		// An attack in progress (from its start to the end of attackDuration, or of the windup), and whether it began this step
		bool is_attacking() const { return m_windup > 0.0f || m_attacking > 0.0f || m_scripted; }
		bool attack_started() const { return m_attackStarted; }
		b2Vec2 get_attack_direction() const { return m_attackDirection; }

		bool is_dead() const { return m_state == EnemyState::Dead; }
		EnemyState get_state() const { return m_state; }
		b2Vec2 get_position() const { return m_body->get_position(); }
		uintptr_t get_id() const { return m_id; }

		Health health;

		// Nothing that blocks sight between it and 'target'
		bool has_line_of_sight(PhysicsWorld2D& world, const b2Vec2& target) const;

	private:

		EnemyConfig m_config;
		uintptr_t m_id;
		EnemyState m_state;
		std::unique_ptr<CharacterController2D> m_body;
		std::vector<b2Vec2> m_path;
		float m_pathTimer;
		float m_attackTimer;
		float m_windup = 0.0f;       // seconds left before the hit of the attack in progress
		float m_attacking = 0.0f;    // seconds left of the attack in progress after its hit
		float m_holdAttacks = 0.0f;  // hurt: no new attack for this long
		float m_knockTimer = 0.0f;   // knocked back: seconds left of the stun
		float m_knockSpeed = 0.0f;   // cm/s, as the original's formula
		float m_knockAccel = 0.0f;
		b2Vec2 m_knockDirection = b2Vec2(0.0f, 0.0f);
		float m_speedScale = 1.0f;
		bool m_scripted = false;     // a scripted attack in progress
		float m_attackSpeed = 0.0f;
		bool m_attackTurns = false;
		b2Vec2 m_facing = b2Vec2(1.0f, 0.0f);
		bool m_attackStarted = false;
		b2Vec2 m_attackDirection = b2Vec2(1.0f, 0.0f);
		uint32 m_random;             // for the random wait between attacks
	};
}
