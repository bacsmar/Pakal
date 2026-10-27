///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: DungeonNet.h
// Purpose: The dungeon played together over the network (TopDown's NetServer and NetClient): what a
//          client needs to build the same dungeon (the seed: the layout comes from it), what only a hero's
//          own player sees (health, mana, level, experience, gold, the abilities' cooldowns), and the
//          server's loop (DungeonServer: a hero for each client that joins, bots for heroes nobody plays,
//          the steps, the snapshots), which the headless DungeonServer program and the tests run, and a
//          player's game runs on a thread of its own to host (DungeonHost). Servers answer LanBrowser's
//          questions for the game "dungeon" (kLanGame), so players find them without an address.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "DungeonGame.h"
#include "net/LanDiscovery.h"
#include "topdown/net/NetServer.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace Pakal
{
	class NavGrid2D;
	class PhysicsWorld2D;
}

namespace Dungeon
{
	// The name servers answer LanBrowser for, and their port when nobody says another
	constexpr const char* kLanGame = "dungeon";
	constexpr uint16_t kDefaultPort = 27960;

	std::vector<uint8_t> game_info(uint32_t seed);
	bool read_game_info(const std::vector<uint8_t>& data, uint32_t& seed);

	// What only a hero's player sees of it
	struct OwnState
	{
		float health = 0.0f, healthMax = 1.0f;
		float mana = 0.0f, manaMax = 1.0f;
		int level = 1, experience = 0, experienceNext = 1, gold = 0;
		std::vector<float> cooldowns;   // share left, by button
		uint32_t unpaid = 0;            // bits: buttons whose ability it cannot pay now
		// what it wears (slot, item) and carries (items), by the rules' ids: each player's own inventory
		std::vector<std::pair<std::string, std::string>> worn;
		std::vector<std::string> bag;
	};
	void write_own(Pakal::ByteWriter& out, const Actor& hero, const DungeonRules& rules);
	bool read_own(const std::vector<uint8_t>& data, OwnState& own);

	class DungeonServer
	{
	public:
		DungeonServer(const DungeonRules& rules, uint32_t seed);
		~DungeonServer();

		// Listens on 'port' (0: any free one) for up to 'maxPlayers'; 'bots' heroes played by the server from the start
		bool start(uint16_t port, size_t maxPlayers, int bots);
		// One step of 1/60 s: who came and went, their inputs, the game, the snapshots
		void step();
		uint16_t port() const { return m_server ? m_server->port() : 0; }
		DungeonGame& game() { return *m_game; }
		Pakal::TopDown::Net::NetServer& server() { return *m_server; }
		uint32_t steps() const { return m_step; }
		static constexpr float kStep = 1.0f / 60.0f;
		// The line its LanBeacon answers with: who hosts, how many play, how far they are
		std::string describe(const std::string& host) const;

	private:
		const DungeonRules& m_rules;
		uint32_t m_seed;
		std::unique_ptr<Pakal::PhysicsWorld2D> m_world;
		std::unique_ptr<Pakal::NavGrid2D> m_nav;
		std::unique_ptr<DungeonGame> m_game;
		std::unique_ptr<Pakal::TopDown::Net::NetServer> m_server;
		std::vector<DungeonBot> m_bots;   // by player index: the first ones are the server's bots
		std::vector<PlayerInput> m_inputs;
		uint32_t m_step = 0;
	};

	// A server on a thread of this program, for a player who hosts: the same steps as DungeonServer's program, on the
	// clock, and found on the network. The host plays as a client of it, on 127.0.0.1.
	class DungeonHost
	{
	public:
		DungeonHost() = default;
		~DungeonHost() { stop(); }
		DungeonHost(const DungeonHost&) = delete;
		DungeonHost& operator=(const DungeonHost&) = delete;

		// Waits until the server listens (false: it could not); 'name' goes in its LanBeacon's line
		bool start(const DungeonRules& rules, uint32_t seed, uint16_t port, size_t maxPlayers, const std::string& name);
		void stop();
		uint16_t port() const { return m_port; }

	private:
		void run(uint32_t seed, uint16_t port, size_t maxPlayers, std::string name);

		DungeonRules m_rules;
		std::thread m_thread;
		std::atomic<bool> m_stop{ false };
		std::atomic<int> m_started{ 0 };   // 1: listening, -1: failed
		uint16_t m_port = 0;
	};
}
