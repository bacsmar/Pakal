///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ZombiesGame.h
// Purpose: The simulation of a Zombies level: the actors and the systems that work across them
//          (bullets, blasts, drops, the counters the level scripts read). It runs on fixed steps and
//          knows nothing about windows or drawing, so the same code runs in the game and in tests.
//          It is the TopDownGame of the Zombies components, plus what only this game has (berserk,
//          money, barricades, escorts, the air strike).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "GameRules.h"
#include "TopDown.h"
#include "topdown/TopDownGame.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace Zombies
{
	// A ground pound (a boomer, the player revived): dust, no fire. An event of this game (TopDownGame::events).
	struct ShockwaveEvent
	{
		b2Vec2 at;
		float radius;
		float shake;
	};

	class ZombiesGame : public TopDownGame
	{
	public:
		ZombiesGame(Pakal::PhysicsWorld2D& world, Pakal::NavGrid2D& nav, const GameRules& rules, uint32_t seed);
		~ZombiesGame() override;

		Pakal::PhysicsWorld2D& world() override { return m_world; }
		Pakal::NavGrid2D& nav() override { return m_nav; }
		const GameRules& rules() const override { return m_rules; }

		// Actors: the world's (SimWorld); the one the player moves
		// The factions: the player and the ones escorted, against the zombies
		const int survivors = factions.add("survivors");
		const int undead = factions.add("zombies");
		bool is_alive(const Actor& actor) const override;
		float hit_radius(const Actor& actor) const override;

		// One fixed step: the actors, then bullets, then removals; then the events of the step (and the ones emitted since
		// the last) are delivered. While the player is down only the player moves on.
		void step(float dt, std::span<const PlayerInput> inputs);
		void step(float dt, const PlayerInput& input) { step(dt, std::span<const PlayerInput>(&input, 1)); }
		// The player is down: everything else stands still (Level::FreezeObjects)
		bool frozen() const;

		// Services for the components
		void fire_bullet(const b2Vec2& from, const b2Vec2& direction, float speed, float damage, float range, float piercing = 1.0f, bool knockback = false, int faction = Factions::None, float height = 1.0f) override;
		// An enemy's shot (meters, seconds): walls stop it, it goes through zombies and hurts the player
		void fire_enemy_bullet(const b2Vec2& from, const b2Vec2& direction, float speed, float damage, float range);
		// Damage from an enemy's blast to the player and to the zombies around 'at' (not to props), but not to 'source'
		void enemy_blast(const b2Vec2& at, float radius, float amount, uintptr_t source);
		// Brings in an enemy of a type (a dog walker lets its dogs loose); set by the level. Returns its id, 0 if none came.
		std::function<uintptr_t(const std::string& type, const b2Vec2& at)> spawnEnemy;

		// The part of the floor the camera shows (an axis-aligned box in level meters, set by the level every frame):
		// zombies far off it walk faster, and the ones left behind are brought to the edge ahead of the player
		void set_screen(const b2Vec2& minCorner, const b2Vec2& maxCorner);
		bool on_screen(const b2Vec2& point) const;
		// Off screen and farther from the player than twice the distance to the screen's farthest corner
		bool far_off_screen(const b2Vec2& point) const;
		// Damage to the enemies and breakable props around 'at' (an explosion), shaking the camera this hard
		void blast(const b2Vec2& at, float radius, float amount, uintptr_t source, float shake = -1.0f) override;
		int tokens = 0;   // tokens picked up (a "collect" mission counts them: pickupCount)
		void drop_pickup(int pickupId, const b2Vec2& at) override;
		void roll_drop(bool random, const std::vector<int>& drops, float rate, const b2Vec2& at) override;
		float random01() override;
		Pakal::RandomStream& random(const std::string& stream) override { return m_random[stream]; }
		void enemy_killed(Actor& enemy);
		// AirStrikeBoost: a burst of blasts around 'center' over the next moments (none lands on the player)
		void airstrike(const b2Vec2& center);

		bool apply_effect(const std::string& effect, Actor& user) override;
		// Money and tokens go to the counters
		bool take_pickup(Actor& taker, const PickupRules& rules) override;
		// Money left on the floor: after a while the nearest free zombie that sees it comes for it
		void pickup_lying(Actor& pickup, float lyingSeconds, float dt) override;

		// A defend mission: the barricades (props the zombies break and the player repairs) and the thing to defend
		std::vector<uintptr_t> barricades;
		uintptr_t objective = 0;
		// An escort mission: the survivor walking with the player (zombies go for whichever of the two is nearer)
		uintptr_t escorted = 0;

		// Script-facing state
		double timeMs = 0.0;     // game time since the level started
		bool berserk = false;    // EnableBerserk
		int difficulty = 0;      // the game variable difficultyLevel: the row of the enemies' tables
		int kills = 0;           // enemies killed (the script's killCount)
		int money = 0;
		int alive_enemies() const;
		void set_berserk(bool on);
		void kill_all_enemies();

		const std::vector<Bullet>& bullets() const { return m_bullets; }

	protected:
		// A hit to an enemy (its brain), a prop or the player
		DamageResult apply_damage(Actor& target, const DamageInfo& hit) override;

	private:
		void advance(float dt, std::span<const PlayerInput> inputs);
		void step_bullets(float dt);
		void entity_removed(Actor& actor) override;
		void call_a_zombie(Actor& coin);

		Pakal::PhysicsWorld2D& m_world;
		Pakal::NavGrid2D& m_nav;
		const GameRules& m_rules;
		std::vector<Bullet> m_bullets;
		struct Strike { float delay; b2Vec2 at; };
		std::vector<Strike> m_strikes;
		void teleport_enemies(float dt);
		bool m_hasScreen = false;
		b2Vec2 m_screenMin = b2Vec2(0.0f, 0.0f), m_screenMax = b2Vec2(0.0f, 0.0f);
		float m_teleportTimer = 0.0f;
		std::map<uintptr_t, float> m_coinCalls;   // money on the floor: seconds to its next call for a zombie
		Pakal::RandomStreams m_random;   // from the level's seed: "sim", "spawn"
	};

	// The Zombies components run only inside a ZombiesGame: they reach its own state through this
	inline ZombiesGame& zombies(TopDownGame& game) { return static_cast<ZombiesGame&>(game); }
}
