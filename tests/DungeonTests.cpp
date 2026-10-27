///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: DungeonTests.cpp
// Purpose: The dungeon prototype without a window: the rules read, the dungeon made from a seed (every
//          room reachable, the same one again from the same seed), the monsters asleep until the hero
//          comes, and the bot clearing it (levels, loot, the boss), the same way each run.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "DungeonGame.h"

#include "box2D/PhysicsWorld2D.h"
#include "gameplay/NavGrid2D.h"

#include <cstdio>
#include <fstream>
#include <memory>

using namespace Dungeon;

namespace
{
	const float kDt = 1.0f / 60.0f;

	struct Run
	{
		bool won = false, lost = false;
		double seconds = 0.0;
		int kills = 0, level = 0, items = 0, gold = 0, rooms = 0, monsters = 0, reachable = 0;
		int startAwake = 0;
		std::string worn;
	};

	Run play(const DungeonRules& rules, uint32_t seed, float limitSeconds)
	{
		Pakal::PhysicsWorld2D world(b2Vec2(0.0f, 0.0f));
		Pakal::NavGrid2D nav(0.5f);
		DungeonGame game(world, nav, rules, seed);
		game.start();
		Run run;
		run.rooms = static_cast<int>(game.layout().rooms.size());
		run.monsters = game.monsters_left();
		const b2Vec2 start = game.player()->position;
		for (const auto& room : game.layout().rooms)
		{
			std::vector<b2Vec2> path;
			if (nav.find_path(start, game.layout().room_center(room), path)) ++run.reachable;
		}
		DungeonBot bot;
		for (int i = 0; i < 2; ++i) game.step(kDt, PlayerInput());
		for (const auto& actor : game.actors())
		{
			const MonsterBrainComponent* brain = actor->get<MonsterBrainComponent>();
			if (brain && brain->awake()) ++run.startAwake;
		}
		while (!game.won() && !game.lost() && game.timeMs < limitSeconds * 1000.0)
		{
			game.step(kDt, bot.think(game, kDt));
		}
		run.won = game.won();
		run.lost = game.lost();
		run.seconds = game.timeMs / 1000.0;
		run.kills = game.kills;
		run.items = game.itemsTaken;
		if (const Actor* hero = game.player())
		{
			run.level = hero->get<ProgressionComponent>()->level();
			run.gold = hero->get<PurseComponent>()->gold;
			for (const auto& [slot, item] : hero->get<EquipmentComponent>()->worn()) run.worn += slot + "=" + item + " ";
		}
		return run;
	}
}

int main(int argc, char** argv)
{
	// Unbuffered: the process ends without flushing stdout (a pipe under ctest), and its lines would be lost
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	int failures = 0;
	auto check = [&](bool ok, const char* message)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
		if (!ok) ++failures;
	};

	const char* file = argc > 1 ? argv[1] : "examples/dungeon_proto/data/dungeon.json";
	std::ifstream in(file);
	const nlohmann::json data = nlohmann::json::parse(in, nullptr, false);
	check(!data.is_discarded(), "the rules are read");
	if (data.is_discarded()) return 1;
	const DungeonRules rules = DungeonRules::from_json(data);
	check(rules.creatures.size() == 5 && rules.items.size() == 7 && rules.buttons.size() == 4 && !rules.boss.empty(), "creatures, items and buttons");

	int cleared = 0;
	for (uint32_t seed : { 1u, 2u, 2026u })
	{
		const Run run = play(rules, seed, 600.0f);
		std::printf("  seed %u: %s after %.1f s, %d of %d monsters, level %d, %d items, %d gold, wearing %s\n", seed,
			run.won ? "CLEARED" : run.lost ? "DIED" : "TIME", run.seconds, run.kills, run.monsters, run.level, run.items, run.gold, run.worn.c_str());
		check(run.rooms == 9 && run.reachable == run.rooms, "every room can be reached from the first");
		check(run.startAwake == 0, "the monsters sleep until the hero comes");
		if (run.won) ++cleared;
		check(run.kills > 0 && run.level >= 2 && run.gold > 0, "the hero fights, grows and gathers gold");
	}
	check(cleared >= 2, "the bot clears most dungeons, the boss last");

	const Run a = play(rules, 2026u, 600.0f), b = play(rules, 2026u, 600.0f);
	check(a.seconds == b.seconds && a.kills == b.kills && a.gold == b.gold && a.worn == b.worn, "the same seed plays the same");

	// Chests: one in the first room; the hero walks to it, it opens once and its loot lies around it; the hero's bag
	// wears and takes off by the commands its player sends (over the network too)
	{
		Pakal::PhysicsWorld2D world(b2Vec2(0.0f, 0.0f));
		Pakal::NavGrid2D nav(0.5f);
		DungeonGame game(world, nav, rules, 2026u);
		game.start(1, true);   // (a client's start, without monsters, has no chests either: they are the server's)
		Actor* hero = game.player();
		Actor* chest = nullptr;
		int chests = 0;
		for (const auto& actor : game.actors())
		{
			if (actor->kind != ActorKind::Prop || actor->name != "chest") continue;
			++chests;
			const auto& first = game.layout().rooms.front();
			const b2Vec2 low = game.layout().center(first.x, first.y), high = game.layout().center(first.x + first.w - 1, first.y + first.h - 1);
			if (actor->position.x >= low.x && actor->position.x <= high.x && actor->position.y >= low.y && actor->position.y <= high.y) chest = actor.get();
		}
		check(chests >= 2 && chest != nullptr, "chests stand in the rooms, one in the first");
		const size_t before = game.actors().size();
		game.step(1.0f / 60.0f, PlayerInput());
		check(chest && !chest->get<ChestComponent>()->opened && game.actors().size() == before, "a chest stays shut while nobody is near");
		if (chest && hero)
		{
			const b2Vec2 near = chest->position + b2Vec2(0.0f, -0.8f);
			if (CharacterBodyComponent* body = hero->get<CharacterBodyComponent>()) body->teleport(near);
			hero->position = near;
			for (int i = 0; i < 3; ++i) game.step(1.0f / 60.0f, PlayerInput());
		}
		int piles = 0;
		for (const auto& actor : game.actors())
		{
			if (actor->kind == ActorKind::Pickup && !actor->removed && chest && (actor->position - chest->position).Length() < 1.5f) ++piles;
		}
		check(chest && chest->get<ChestComponent>()->opened && chest->get<ChestComponent>()->view_state().dead && game.chestsOpened == 1,
			"the hero near it opens it (once)");
		check(piles >= 1 || (hero && hero->get<PurseComponent>()->gold > 0) || game.itemsTaken > 0, "its loot spills around it (or the hero took it at once)");

		EquipmentComponent* gear = hero ? hero->get<EquipmentComponent>() : nullptr;
		if (gear)
		{
			gear->take("chain_mail");
			gear->take("swift_ring");
			Pakal::ByteReader wear(DungeonGame::gear_command(DungeonGame::GearCommand::Wear, "chain_mail"));
			DungeonGame::apply_gear_command(*hero, wear);
			Pakal::ByteReader ring(DungeonGame::gear_command(DungeonGame::GearCommand::Wear, "swift_ring"));
			DungeonGame::apply_gear_command(*hero, ring);
			const bool worn = gear->worn().count("armor") && gear->worn().at("armor") == "chain_mail" && gear->worn().count("ring");
			Pakal::ByteReader off(DungeonGame::gear_command(DungeonGame::GearCommand::TakeOff, "ring"));
			DungeonGame::apply_gear_command(*hero, off);
			check(worn && !gear->worn().count("ring"), "a command wears an item of the bag, another takes it off");
		}
	}

	std::printf("%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures == 0 ? 0 : 1;
}
