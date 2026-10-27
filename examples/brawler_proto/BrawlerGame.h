///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: BrawlerGame.h
// Purpose: A one-arena brawler on the TopDown module, to try the module with a second game: its rules
//          (fighters and their moves, read from brawler.json), its simulation (a TopDownGame) and the
//          enemies' brain. Draws nothing; BrawlerLevelState shows it.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "RandomStreams.h"
#include "topdown/TopDownGame.h"
#include "topdown/kits/brawler/FighterComponent.h"

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

namespace Brawler
{
	using namespace Pakal::TopDown;

	// A kind of fighter: its body, its moves and, for an enemy, how it fights
	struct FighterRules
	{
		float health = 100.0f;
		float runSpeed = 3.0f;      // meters per second
		float radius = 0.35f;       // its body (meters)
		float tall = 1.8f;
		float blockFactor = 0.2f;   // what a hit does to it while it guards
		std::vector<MoveRules> moves;
		// An enemy: it closes in to this distance in front of the player and does one of 'attacks' every so often
		float attackRange = 1.0f;
		float waitMin = 0.6f, waitMax = 1.4f;
		std::vector<std::string> attacks;
		nlohmann::json look;        // how it is drawn (CharacterViewComponent::Look), read by the level
	};

	struct BrawlerRules : TopDownRules
	{
		float halfWidth = 8.0f, halfDepth = 3.5f;   // the arena: x from -halfWidth to halfWidth, y likewise (meters)
		std::map<std::string, FighterRules> fighters;
		std::string player = "hero";
		b2Vec2 playerAt = b2Vec2(-4.0f, 0.0f);
		struct Spawn { std::string type; b2Vec2 at; };
		std::vector<Spawn> enemies;
		// The player's buttons: each one tries its moves in order (the first one that can be done now)
		std::map<std::string, std::vector<std::string>> buttons;
		nlohmann::json visuals;     // the camera, the floor and the props (the level's)

		static BrawlerRules from_json(const nlohmann::json& data);
	};

	// The brawler's buttons (PlayerInput::held and pressed): their bits, and their names in the rules' "buttons"
	enum Button : int { Light, Heavy, Jump, Dodge, Block, ButtonCount };
	inline const char* button_name(int button)
	{
		static const char* names[] = { "light", "heavy", "jump", "dodge", "block" };
		return button >= 0 && button < ButtonCount ? names[button] : "";
	}

	class BrawlerGame : public TopDownGame
	{
	public:
		BrawlerGame(Pakal::PhysicsWorld2D& world, Pakal::NavGrid2D& nav, const BrawlerRules& rules, uint32_t seed);
		~BrawlerGame() override;

		// The player and the enemies of the rules, and the arena's walls
		void start();
		// One step: the player's input (walking; its buttons do their moves)
		void step(float dt, const PlayerInput& input);
		Actor& add_fighter(const std::string& type, const b2Vec2& at, bool player);

		int enemies_left() const;
		bool won() const { return enemies_left() == 0; }
		bool lost() const;
		double timeMs = 0.0;

		// The heroes against the thugs
		const int heroes = factions.add("heroes");
		const int thugs = factions.add("thugs");

		Pakal::PhysicsWorld2D& world() override { return m_world; }
		Pakal::NavGrid2D& nav() override { return m_nav; }
		const BrawlerRules& rules() const override { return m_rules; }
		bool is_alive(const Actor& actor) const override;
		float hit_radius(const Actor& actor) const override;
		float random01() override { return m_random["sim"].uniform(); }
		Pakal::RandomStream& random(const std::string& stream) override { return m_random[stream]; }


	protected:
		DamageResult apply_damage(Actor& target, const DamageInfo& hit) override;
		void entity_removed(Actor& actor) override;

	private:
		Pakal::PhysicsWorld2D& m_world;
		Pakal::NavGrid2D& m_nav;
		const BrawlerRules& m_rules;
		Pakal::RandomStreams m_random;
		std::map<uintptr_t, float> m_radius;   // each fighter's body
	};

	// An enemy: it walks up to the nearest player, to one side of it, and attacks every so often
	class BrainComponent : public Component
	{
	public:
		explicit BrainComponent(const FighterRules& rules) : m_rules(rules) {}
		void update(TopDownGame& game, float dt) override;

	private:
		const FighterRules& m_rules;
		float m_wait = 0.8f;
	};

	// A bot for the player (tests, BRAWLER_AUTOPLAY): it walks up to the nearest enemy and hits it
	PlayerInput autoplay(BrawlerGame& game);
}
