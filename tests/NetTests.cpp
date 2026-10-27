///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: NetTests.cpp
// Purpose: The network without a window: bytes written and read back (and a broken message stopping the
//          reader), an input and a snapshot through their codecs (names only until known), the world
//          drawn between two snapshots, and a real game over UDP on this machine: the dungeon's server
//          and two clients that join, walk their heroes with their inputs, see each other in their
//          snapshots and their own mana, and one that leaves. Then a game found on the network (a
//          LanBeacon answering a LanBrowser) and a game hosted on a thread (DungeonHost) and joined.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "DungeonNet.h"
#include "topdown/net/NetClient.h"
#include "topdown/net/ReplicaGame.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <thread>

using namespace Pakal;
using namespace Pakal::TopDown;
using namespace Pakal::TopDown::Net;

int main(int argc, char** argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	int failures = 0;
	auto check = [&](bool ok, const char* message)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
		if (!ok) ++failures;
	};

	// Bytes
	{
		ByteWriter out;
		out.varuint(5);
		out.varuint(300000);
		out.varint(-3);
		out.quantized(-12.345f, 0.01f);
		out.angle(3.14159265f);
		out.string("zombie");
		out.f32(1.5f);
		ByteReader in(out.data());
		check(in.varuint() == 5 && in.varuint() == 300000 && in.varint() == -3 && std::fabs(in.quantized(0.01f) + 12.35f) < 0.006f
			&& std::fabs(in.angle() - 3.14159265f) < 0.03f && in.string() == "zombie" && in.f32() == 1.5f && in.ok() && in.done(),
			"numbers, quantized values, angles and strings come back");
		check(out.size() < 24, "small numbers take few bytes");
		ByteReader broken(out.data().data(), 3);
		broken.varuint();
		broken.varuint();
		broken.string();
		check(!broken.ok(), "a message cut short stops the reader");
	}

	// An input and a snapshot through their codecs
	{
		PlayerInput input;
		input.move = b2Vec2(0.6f, -0.8f);
		input.hasAimPoint = true;
		input.aimPoint = b2Vec2(10.25f, -3.5f);
		input.fire = true;
		input.held = 5;
		input.pressed = 4;
		ByteWriter out;
		write_input(out, input);
		ByteReader in(out.data());
		const PlayerInput back = read_input(in);
		check(std::fabs(back.move.x - 0.6f) < 0.01f && std::fabs(back.move.y + 0.8f) < 0.01f && back.hasAimPoint && back.fire
			&& std::fabs(back.aimPoint.x - 10.25f) < 0.006f && back.held == 5 && back.pressed == 4, "an input comes back");

		Snapshot snapshot;
		snapshot.step = 42;
		ActorState hero;
		hero.id = 7;
		hero.kind = 0;
		hero.faction = 1;
		hero.name = "hero";
		hero.position = b2Vec2(3.0f, 4.0f);
		hero.health = 0.5f;
		hero.attacking = true;
		hero.action = "strike";
		hero.actionSerial = 3;
		snapshot.actors.push_back(hero);
		std::map<uintptr_t, ActorState> known;
		ByteWriter first, second;
		write_snapshot(first, snapshot, [](uintptr_t) { return false; });
		write_snapshot(second, snapshot, [](uintptr_t) { return true; });
		Snapshot a, b;
		ByteReader inFirst(first.data()), inSecond(second.data());
		check(read_snapshot(inFirst, a, known) && read_snapshot(inSecond, b, known) && b.actors.size() == 1 && b.actors[0].name == "hero"
			&& b.actors[0].faction == 1 && std::fabs(b.actors[0].health - 0.5f) < 0.01f && b.actors[0].action == "strike" && b.actors[0].attacking,
			"a snapshot comes back, and a name sent once is remembered");
		check(second.size() < first.size(), "a snapshot without the names is smaller");

		// between two snapshots
		SnapshotBuffer buffer(3.0f);
		Snapshot s1 = snapshot, s2 = snapshot;
		s1.step = 100;
		s2.step = 103;
		s2.actors[0].position = b2Vec2(6.0f, 4.0f);
		buffer.add(s1);
		buffer.add(s2);
		buffer.advance(0.0f);   // starts 3 steps behind the latest: at the first
		buffer.advance(1.5f);
		std::vector<ActorState> drawn;
		check(buffer.sample(drawn) && drawn.size() == 1 && std::fabs(drawn[0].position.x - 4.5f) < 0.3f, "the world is drawn between two snapshots");
	}

	// A game over UDP on this machine
	{
		const char* file = argc > 1 ? argv[1] : "examples/dungeon_proto/data/dungeon.json";
		std::ifstream in(file);
		const Dungeon::DungeonRules rules = Dungeon::DungeonRules::from_json(nlohmann::json::parse(in, nullptr, false));
		Dungeon::DungeonServer server(rules, 2026);
		check(server.start(0, 4, 0), "the server listens");
		NetClient one, two;
		check(one.connect("127.0.0.1", server.port(), "one") && two.connect("127.0.0.1", server.port(), "two"), "two clients connect");
		ReplicaGame replica;
		PlayerInput walkEast;
		walkEast.move = b2Vec2(1.0f, 0.0f);
		b2Vec2 heroStart(0.0f, 0.0f);
		bool started = false;
		// a second of play, in real time (the packets go through the loopback)
		for (int i = 0; i < 180; ++i)
		{
			server.step();
			one.poll(Dungeon::DungeonServer::kStep);
			two.poll(Dungeon::DungeonServer::kStep);
			if (one.welcomed() && !started)
			{
				started = true;
				if (Actor* hero = static_cast<Actor*>(server.game().find(one.welcome().actor))) heroStart = hero->position;
			}
			if (i > 60) one.send_input(walkEast);
			two.send_input(PlayerInput());
			replica.apply(one.actors());
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		uint32_t seed = 0;
		check(one.welcomed() && two.welcomed() && server.server().clients() == 2 && server.game().players().size() == 2
			&& Dungeon::read_game_info(one.welcome().game, seed) && seed == 2026, "both join as heroes, and learn the dungeon's seed");
		const Actor* heroOne = static_cast<Actor*>(server.game().find(one.welcome().actor));
		check(heroOne && heroOne->position.x - heroStart.x > 1.5f, "the first one's input walks its hero");
		check(one.snapshots() > 30 && two.snapshots() > 30, "snapshots come to both");
		Actor* seen = replica.replica(one.welcome().actor);
		Actor* other = replica.replica(two.welcome().actor);
		std::printf("  hero on the server at %.2f, drawn at %.2f (%u snapshots)\n", heroOne ? heroOne->position.x : 0.0f, seen ? seen->position.x : 0.0f, one.snapshots());
		check(seen && other && heroOne && std::fabs(seen->position.x - heroOne->position.x) < 0.8f, "a client draws its hero near where the server has it, and the other hero");
		check(replica.actors().size() > 40, "and the monsters");
		Dungeon::OwnState own;
		check(Dungeon::read_own(one.own(), own) && own.manaMax > 20.0f && own.level == 1 && own.cooldowns.size() == 4, "a hero's own data comes to its player");
		check(!own.worn.empty() || !own.bag.empty(), "and its inventory");

		// its player wears an item of its bag: a command to the server, which the next snapshots show
		Actor* serverHero = nullptr;
		for (Actor* player : server.game().players())
		{
			if (player->id == one.welcome().actor) serverHero = player;
		}
		if (serverHero) serverHero->get<EquipmentComponent>()->take("swift_ring");
		one.send_command(Dungeon::DungeonGame::gear_command(Dungeon::DungeonGame::GearCommand::Wear, "swift_ring"));
		bool wears = false;
		for (int i = 0; i < 120 && !wears; ++i)
		{
			server.step();
			one.poll(Dungeon::DungeonServer::kStep);
			two.poll(Dungeon::DungeonServer::kStep);
			Dungeon::OwnState now;
			if (Dungeon::read_own(one.own(), now))
			{
				for (const auto& [slot, id] : now.worn) wears = wears || id == "swift_ring";
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		check(wears, "a player's command wears an item on the server, and its snapshots show it worn");

		two.close();
		for (int i = 0; i < 60 && server.server().clients() > 1; ++i)
		{
			server.step();
			one.poll(Dungeon::DungeonServer::kStep);
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		for (int i = 0; i < 30; ++i)
		{
			server.step();
			one.poll(Dungeon::DungeonServer::kStep);
			replica.apply(one.actors());
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
		}
		replica.forget_missing();
		check(server.server().clients() == 1 && !replica.replica(two.welcome().actor), "one leaves: its hero goes, for the other too");
	}

	// Finding a game on the network: a beacon answers a browser's question (on ports of their own, not the games' ones)
	{
		const uint16_t question = 27971;
		LanBeacon beacon;
		LanBrowser browser;
		check(beacon.open("dungeon", 4242, question) && browser.open("dungeon", question), "a beacon and a browser open");
		beacon.set_info("ana  -  1 player");
		LanBeacon other;
		other.open("another game", 4343, question);
		double now = 0.0;
		for (int i = 0; i < 100 && browser.found().empty(); ++i)
		{
			if (i % 20 == 0) browser.search(now);
			beacon.poll();
			other.poll();
			browser.poll(now);
			now += 0.01;
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		const auto& found = browser.found();
		check(found.size() == 1 && found[0].port == 4242 && found[0].info == "ana  -  1 player", "the browser finds the game, its port and its line (once)");
		browser.poll(now + 10.0);
		check(browser.found().empty(), "a game that stops answering is forgotten");
	}

	// A player who hosts: the server on a thread of its own, joined on 127.0.0.1
	{
		const char* file = argc > 1 ? argv[1] : "examples/dungeon_proto/data/dungeon.json";
		std::ifstream in(file);
		const Dungeon::DungeonRules rules = Dungeon::DungeonRules::from_json(nlohmann::json::parse(in, nullptr, false));
		Dungeon::DungeonHost host;
		check(host.start(rules, 7, 0, 4, "tester") && host.port() != 0, "a game hosts on a thread");
		NetClient client;
		client.connect("127.0.0.1", host.port(), "me");
		for (int i = 0; i < 300 && client.snapshots() < 5; ++i)
		{
			client.poll(0.01f);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		uint32_t seed = 0;
		check(client.welcomed() && Dungeon::read_game_info(client.welcome().game, seed) && seed == 7 && client.snapshots() >= 5, "its player joins it and gets snapshots");
		client.close();
		host.stop();
	}

	std::printf("%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures == 0 ? 0 : 1;
}
