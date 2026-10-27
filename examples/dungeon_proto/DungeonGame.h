///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: DungeonGame.h
// Purpose: A small Diablo-like on the TopDown module and its RPG kit, the third game that tries them: its
//          rules (dungeon.json: the hero, the monsters, their abilities, the items and the loot), a
//          dungeon of rooms and corridors made from the seed, its simulation (a TopDownGame: experience,
//          loot on the floor, the boss), the monsters' brain and a bot. Draws nothing; DungeonLevelState
//          shows it.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "RandomStreams.h"
#include "net/ByteStream.h"
#include "topdown/TopDownGame.h"
#include "topdown/kits/rpg/RpgComponents.h"

#include <nlohmann/json.hpp>

#include <map>
#include <string>
#include <vector>

namespace Dungeon
{
	using namespace Pakal::TopDown;

	// A kind of character: its body, its abilities and, for a monster, how it fights and what it is worth
	struct CreatureRules
	{
		float health = 60.0f;
		float runSpeed = 3.0f;           // meters per second
		float radius = 0.35f;
		std::vector<AbilityRules> abilities;
		// a monster: it wakes when the hero is this near and in sight, and uses its first ability within 'attackRange' (a ranged
		// one: its last ability between 'attackRange' and 'shootRange')
		float aggro = 8.0f;
		float attackRange = 1.2f;
		float shootRange = 0.0f;
		float waitMin = 0.4f, waitMax = 1.0f;
		int experience = 10;             // what defeating it gives
		std::string loot;                // its loot table
		bool boss = false;
		nlohmann::json look;             // how it is drawn (CharacterViewComponent::Look), read by the level
	};

	// The rooms of a dungeon on a grid, joined by corridors; the floor in cells
	struct DungeonLayout
	{
		float cell = 2.0f;               // meters a side
		int width = 0, height = 0;       // cells
		std::vector<uint8_t> floor;      // 1: walkable
		struct Room { int x, y, w, h; int depth = 0; };   // cells; 'depth': rooms from the first
		std::vector<Room> rooms;

		bool is_floor(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height && floor[static_cast<size_t>(y * width + x)] != 0; }
		// The center of a cell, meters (the dungeon's corner is at 0, 0)
		b2Vec2 center(int x, int y) const { return b2Vec2((static_cast<float>(x) + 0.5f) * cell, (static_cast<float>(y) + 0.5f) * cell); }
		b2Vec2 room_center(const Room& room) const;
	};

	struct DungeonRules : TopDownRules
	{
		// the dungeon: 'columns' x 'rows' rooms of 'roomMin'..'roomMax' cells, 'spacing' cells apart, corridors 'corridor' wide
		int columns = 3, rows = 3;
		int roomMin = 4, roomMax = 6, spacing = 9, corridor = 2;
		float cell = 2.0f;
		float extraDoors = 0.3f;         // the chance of a corridor besides the ones that join every room
		// monsters: how many per room (more the deeper), of which types, and the boss in the deepest room
		int monstersBase = 2;
		float monstersPerDepth = 1.0f;
		std::vector<std::string> monsterTypes;
		std::string boss;
		// chests: the chance of one in each room (the first room always has one), their loot table and how many times it is
		// rolled when one opens
		float chestChance = 0.6f;
		std::string chestLoot = "chest";
		int chestRolls = 2;
		std::string hero = "hero";
		std::map<std::string, CreatureRules> creatures;
		ProgressionRules progression;
		float manaRegen = 2.0f;          // the hero's, per second
		std::map<std::string, ItemRules> items;
		std::map<std::string, LootTable> loot;
		std::vector<std::string> startItems;   // worn from the start
		// The hero's buttons (PlayerInput::held and pressed): the ability each one uses
		std::vector<std::string> buttons;
		nlohmann::json visuals;          // camera, look, the pieces of the dungeon

		static DungeonRules from_json(const nlohmann::json& data);
	};

	// A pile on the floor: gold or an item, the hero takes it walking over it
	class LootComponent : public Component
	{
	public:
		LootComponent(int gold, std::string item) : gold(gold), item(std::move(item)) {}
		void update(TopDownGame& game, float dt) override;

		const int gold;
		const std::string item;
	};

	// A chest: the first hero that comes near opens it, and its loot spills around it. Its model shows it open (it is
	// the model's CharacterViewSource: 'dead' is open, which the snapshots carry too).
	class ChestComponent : public Component, public CharacterViewSource
	{
	public:
		void update(TopDownGame& game, float dt) override;
		CharacterViewState view_state() const override;
		bool opened = false;
	};

	class DungeonGame : public TopDownGame
	{
	public:
		DungeonGame(Pakal::PhysicsWorld2D& world, Pakal::NavGrid2D& nav, const DungeonRules& rules, uint32_t seed);
		~DungeonGame() override;

		// Makes the dungeon from the seed (rooms, corridors, walls, walkable grid), 'heroes' heroes in the first room (each one
		// a player) and, with 'monsters', the monsters. A client of a server makes only the dungeon (start(0, false)).
		void start(int heroes = 1, bool monsters = true);
		// Another hero (a player that joins), in the first room
		Actor& add_hero();
		// One step: each player's input (walking, aiming; the trigger is the first ability, each button its ability)
		void step(float dt, std::span<const PlayerInput> inputs);
		void step(float dt, const PlayerInput& input) { step(dt, std::span<const PlayerInput>(&input, 1)); }
		Actor& add_creature(const std::string& type, const b2Vec2& at, bool hero);
		// Where an ability goes when its player does not aim: the nearest living monster within 9 m in sight, or where the
		// hero faces
		b2Vec2 auto_aim(const Actor& hero);

		const DungeonLayout& layout() const { return m_layout; }
		const CreatureRules* creature(const Actor& actor) const;
		int monsters_left() const;
		bool boss_down() const { return m_bossDown; }
		bool won() const { return m_bossDown; }
		bool lost() const;
		double timeMs = 0.0;
		int kills = 0;
		int itemsTaken = 0;
		int chestsOpened = 0;
		// A hero opens a chest: its loot spills around it
		void open_chest(Actor& chest);
		// A player's command on its inventory (also over the network: NetServer::Hooks::command): wear an item of its bag,
		// or take off what a slot wears
		enum class GearCommand : uint8_t { Wear = 1, TakeOff = 2 };
		static std::vector<uint8_t> gear_command(GearCommand command, const std::string& what);
		static void apply_gear_command(Actor& hero, Pakal::ByteReader& in);
		// Something the interface tells the player (an item taken, a level up), and how many there have been
		std::string message;
		int messages = 0;
		void say(const std::string& text) { message = text; ++messages; }

		// The hero's side and the monsters'
		const int heroes = factions.add("heroes");
		const int monsters = factions.add("monsters");

		Pakal::PhysicsWorld2D& world() override { return m_world; }
		Pakal::NavGrid2D& nav() override { return m_nav; }
		const DungeonRules& rules() const override { return m_rules; }
		bool is_alive(const Actor& actor) const override;
		float hit_radius(const Actor& actor) const override;
		float random01() override { return m_random["sim"].uniform(); }
		Pakal::RandomStream& random(const std::string& stream) override { return m_random[stream]; }

	protected:
		DamageResult apply_damage(Actor& target, const DamageInfo& hit) override;
		void entity_removed(Actor& actor) override;

	private:
		void make_layout();
		void build_walls();
		void populate(int heroes, bool monsters);
		void drop_loot(const std::string& table, const b2Vec2& at, int rolls = 1, float scatter = 0.5f);

		Pakal::PhysicsWorld2D& m_world;
		Pakal::NavGrid2D& m_nav;
		const DungeonRules& m_rules;
		Pakal::RandomStreams m_random;
		DungeonLayout m_layout;
		std::map<uintptr_t, std::string> m_types;   // each creature's type
		bool m_bossDown = false;
	};

	// A monster: asleep until the hero comes near and in sight, then it walks to it along the walkable grid and uses its
	// abilities on it (a ranged one keeps its distance)
	class MonsterBrainComponent : public Component
	{
	public:
		explicit MonsterBrainComponent(const CreatureRules& rules) : m_rules(rules) {}
		void update(TopDownGame& game, float dt) override;
		bool awake() const { return m_awake; }

	private:
		const CreatureRules& m_rules;
		bool m_awake = false;
		float m_wait = 0.5f;
		float m_repath = 0.0f;
		std::vector<b2Vec2> m_path;
		size_t m_next = 0;
	};

	// Walks along the walkable grid toward 'goal': the direction to move this step (zero when there). 'path' and 'next' are
	// kept between steps; 'repath' counts down to the next search.
	b2Vec2 follow_path(TopDownGame& game, const b2Vec2& from, const b2Vec2& goal, std::vector<b2Vec2>& path, size_t& next, float& repath, float dt);

	// A bot for the hero (tests, DUNGEON_AUTOPLAY): it goes to the nearest monster, room after room, uses its abilities, heals
	// when low, and takes the loot it sees
	class DungeonBot
	{
	public:
		// The input of player 'index' for this step
		PlayerInput think(DungeonGame& game, float dt, size_t index = 0);

	private:
		std::vector<b2Vec2> m_path;
		size_t m_next = 0;
		float m_repath = 0.0f;
	};
}
