///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Components.h
// Purpose: The components of the Zombies actors. Gameplay values come from GameRules (the original
//          game's data); the components turn them into behavior.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "TopDown.h"
#include "topdown/TopDownComponents.h"
#include "topdown/TopDownGame.h"
#include "GameRules.h"
#include "topdown/Health.h"

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Pakal::TopDown
{
	class EnemyAI2D;
	struct EnemyConfig;
}

namespace Zombies
{
	class ZombiesGame;

	// A zombie: EnemyAI2D with the rules of its type, berserk, death, drops and fading away. The special attacks (boomer,
	// tourist, dog walker, dog, zombinator) run here, step by step as the original's attack components.
	class EnemyBrainComponent : public Component, public CharacterViewSource
	{
	public:
		using Action = CharacterAction;

		EnemyBrainComponent(ZombiesGame& game, const std::string& type, const b2Vec2& at, uintptr_t id);
		~EnemyBrainComponent() override;

		// Takes a hit (ZombiesGame::apply_damage). Returns the damage taken: 0 when dead or a bear that cannot be hurt now.
		float damage(ZombiesGame& game, const DamageInfo& hit);
		void kill(ZombiesGame& game);
		bool knocked_back() const;
		float knock_seconds_left() const;
		// Seconds an attack of this zombie lasts (its clip, or its windup)
		float get_attack_duration() const;
		void teleport(const b2Vec2& at);
		void set_berserk(bool on);
		bool is_dead() const;
		bool is_attacking() const;
		bool attack_started() const;
		b2Vec2 attack_direction() const;
		float dead_seconds() const { return m_deadSeconds; }
		const EnemyRules& rules() const { return *m_rules; }
		const std::string& type() const { return m_type; }
		b2Vec2 position() const;
		bool berserk() const { return m_berserk; }
		bool hurt_recently() const { return m_hurtTimer > 0.0f; }
		// How many times its hurt clip started (at most once every 2 s): the model plays it when this changes
		int hurt_clips() const { return m_hurtClips; }
		// Its health bar shows while this is above 0 (seconds), and its share of health
		float health_bar_seconds() const { return m_healthBarTimer; }
		float health_share() const;
		// Runs one of the special attacks (not a swing or a flash)
		bool special_attack() const { return m_rules->attack != EnemyAttack::Melee && m_rules->attack != EnemyAttack::Flash; }
		const Action& action() const { return m_action; }
		// Blown to pieces (a tourist's explosion, a dog's bite): no body is left
		bool burst() const { return m_burst; }
		b2Vec2 facing() const;
		// Money left on the floor: a zombie goes for it (go_for_coin, the coin's actor), takes it and runs off with it,
		// then chases again; it drops all it carries when it dies
		bool free_for_coin() const;
		bool sees(ZombiesGame& game, const b2Vec2& at) const;   // within its sight range, nothing in the way
		void go_for_coin(uintptr_t coin);
		uintptr_t coin() const { return m_coin; }
		int coins_carried() const { return static_cast<int>(m_carried.size()); }
		// A body flung by a melee kill: meters above the floor, and how far it has tumbled (radians)
		float lift() const { return m_lift; }
		float tumble() const { return m_tumble; }

		void update(TopDownGame& game, float dt) override;
		CharacterViewState view_state() const override;

		// The physics and attack values of an enemy type (rules in game units -> meters and seconds), on the game's layers
		static EnemyConfig config_for(const EnemyRules& rules, bool berserk, int difficulty, const TopDownLayers& layers);

	private:
		void on_death(ZombiesGame& game);
		// Defend and escort missions: a barricade in its way, the thing to defend or the one escorted instead of the player
		void choose_victim(ZombiesGame& game, Actor* player, b2Vec2& target, Actor*& victim);
		void update_special(ZombiesGame& game, float dt, Actor* player);
		void explode(ZombiesGame& game);   // the tourist blows up
		void end_special();
		void show(const std::string& name, float seconds, bool loop);

		// The steps of the special attacks
		enum class Phase { None, Windup, Recover, Prepare, Explode, Releasing, Spawn, Run, Pounce, InAir, Windmill, Sleep, Wake, Rear };
		// The bear's own steps (asleep, waking, rearing up): it stands still and takes no damage
		bool bear_still() const { return m_phase == Phase::Sleep || m_phase == Phase::Wake || m_phase == Phase::Rear; }
		bool m_reared = false;           // the bear stands and swings its chain
		float m_wokeHealth = 0.0f;       // its health when it woke (it rears up after losing a share of it)

		std::string m_type;
		TopDownLayers m_layers;          // the game's collision layers (its body, what blocks its sight)
		const EnemyRules* m_rules;
		std::unique_ptr<EnemyAI2D> m_ai;
		bool m_berserk = false;
		int m_difficulty = 0;
		bool m_deathHandled = false;
		float m_deadSeconds = 0.0f;
		float m_hurtTimer = 0.0f;
		float m_hurtClipTimer = 0.0f;    // no other hurt clip until it runs out
		int m_hurtClips = 0;
		float m_healthBarTimer = 0.0f;
		Phase m_phase = Phase::None;
		float m_phaseSeconds = 0.0f;     // in the current phase
		bool m_burst = false;
		bool m_flung = false;            // flying from a melee kill
		b2Vec2 m_flingVelocity = b2Vec2(0.0f, 0.0f);
		float m_lift = 0.0f;
		float m_liftSpeed = 0.0f;
		float m_tumble = 0.0f;
		bool m_exploded = false;         // the tourist's blast went off (it happens once)
		std::vector<uintptr_t> m_dogs;   // a dog walker's dogs
		int m_dogsReleased = 0;
		double m_lastReleaseMs = -1.0e9;
		bool m_dogOut = false;           // the dog of the release in progress came out
		Action m_action;
		uintptr_t m_coin = 0;            // the coin it goes for
		float m_coinSeconds = 0.0f;      // since it set off for it (it gives up after a while)
		float m_fleeSeconds = 0.0f;      // running off with a coin
		std::vector<int> m_carried;      // the pickups it took (dropped when it dies)
	};
}
