///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: DungeonServerMain.cpp
// Purpose: DungeonServer, the dungeon's headless server: no window, no graphics, only the simulation and
//          the network. Players find it from the dungeon's menu (it answers LanBrowser), or join with
//          PakalPlayer DungeonModule.dll and DUNGEON_CONNECT=host:port.
//
//          DungeonServer [--port 27960] [--seed 2026] [--players 4] [--bots 0] [--seconds 0]
//                        [--rules Assets/dungeon/dungeon.json]
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "DungeonNet.h"

#include "LogMgr.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>

int main(int argc, char** argv)
{
	uint16_t port = 27960;
	uint32_t seed = 2026;
	size_t players = 4;
	int bots = 0;
	double seconds = 0.0;   // 0: until stopped
	std::string rulesFile = "Assets/dungeon/dungeon.json";
	for (int i = 1; i + 1 < argc; i += 2)
	{
		const std::string option = argv[i], value = argv[i + 1];
		if (option == "--port") port = static_cast<uint16_t>(std::atoi(value.c_str()));
		else if (option == "--seed") seed = static_cast<uint32_t>(std::atoll(value.c_str()));
		else if (option == "--players") players = static_cast<size_t>(std::atoi(value.c_str()));
		else if (option == "--bots") bots = std::atoi(value.c_str());
		else if (option == "--seconds") seconds = std::atof(value.c_str());
		else if (option == "--rules") rulesFile = value;
		else
		{
			std::fprintf(stderr, "unknown option %s\n", option.c_str());
			return 1;
		}
	}
	std::ifstream file(rulesFile);
	const nlohmann::json data = nlohmann::json::parse(file, nullptr, false);
	if (!file || data.is_discarded())
	{
		std::fprintf(stderr, "cannot read the rules '%s'\n", rulesFile.c_str());
		return 1;
	}
	const Dungeon::DungeonRules rules = Dungeon::DungeonRules::from_json(data);
	Dungeon::DungeonServer server(rules, seed);
	if (!server.start(port, players, bots)) return 1;
	Pakal::LanBeacon beacon;
	if (!beacon.open(Dungeon::kLanGame, server.port())) std::printf("(the discovery port %u is taken: players must use DUNGEON_CONNECT)\n", static_cast<unsigned>(Pakal::kLanDiscoveryPort));
	const char* computer = std::getenv("COMPUTERNAME");
	const std::string name = std::string(computer ? computer : "server") + " (DungeonServer)";
	std::printf("DungeonServer: seed %u on port %u, up to %zu players, %d bots\n", seed, static_cast<unsigned>(server.port()), players, bots);

	// steps of 1/60 s on the clock: late ones are caught up (at most a quarter of a second at once)
	using Clock = std::chrono::steady_clock;
	const auto step = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(Dungeon::DungeonServer::kStep));
	auto next = Clock::now();
	auto report = Clock::now();
	bool over = false;
	while (seconds <= 0.0 || server.steps() * Dungeon::DungeonServer::kStep < seconds)
	{
		const auto now = Clock::now();
		if (now - next > std::chrono::milliseconds(250)) next = now;
		while (next <= now)
		{
			server.step();
			next += step;
		}
		if (now - report > std::chrono::seconds(5))
		{
			report = now;
			std::printf("  %.0f s: %zu clients, %d monsters left, %u snapshots sent\n", server.steps() * Dungeon::DungeonServer::kStep,
				server.server().clients(), server.game().monsters_left(), server.server().snapshots_sent());
		}
		if (!over && (server.game().won() || server.game().lost()))
		{
			over = true;
			std::printf("  %s after %.1f s\n", server.game().won() ? "DUNGEON CLEARED" : "EVERY HERO DOWN", server.game().timeMs / 1000.0);
		}
		beacon.set_info(server.describe(name));
		beacon.poll();
		std::this_thread::sleep_until(next);
	}
	return 0;
}
