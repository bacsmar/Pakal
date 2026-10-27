///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: NetServer.h
// Purpose: A TopDown game played over the network, the server's side: it lets clients join (the game
//          makes each one a player's character), takes each one's input for the step, and sends each
//          one snapshots of the world every few steps (with the data only its own player has). The game
//          runs as always; the server only stands at the start and the end of its step:
//              server.poll(); server.fill_inputs(inputs); game.step(...); server.end_step(step);
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "net/NetHost.h"
#include "topdown/net/NetProtocol.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace Pakal::TopDown::Net
{
	class NetServer
	{
	public:
		struct Hooks
		{
			// A client wants to play: its character (made a player of the game), or nullptr to refuse it
			std::function<Actor*(uint32_t peer, const std::string& name)> join;
			// A client left: what becomes of its character is the game's
			std::function<void(uint32_t peer, Actor& character)> leave;
			// What only this player sees, in each of its snapshots (its mana, its gold...)
			std::function<void(ByteWriter& out, const Actor& character)> own;
			// A command of this player (NetClient::send_command): the game reads its bytes
			std::function<void(Actor& character, ByteReader& in)> command;
		};

		// 'game': what the client needs to build the world as the server did (a seed); the step rate and snapshots every
		// 'snapshotEvery' steps
		NetServer(TopDownGame& game, Hooks hooks, std::vector<uint8_t> gameInfo, uint32_t stepHz = 60, uint32_t snapshotEvery = 3);

		bool listen(uint16_t port, size_t maxClients);
		uint16_t port() const { return m_host.port(); }
		void close() { m_host.close(); }

		// Clients that came and went, and their inputs
		void poll();
		// The inputs of this step, in the order of the game's players: a remote player's latest (each button press used
		// once). The others' entries are left as they are.
		void fill_inputs(std::vector<PlayerInput>& inputs);
		// After the game's step: a snapshot to each client when it is time
		void end_step(uint32_t step);

		size_t clients() const { return m_clients.size(); }
		uint32_t snapshots_sent() const { return m_snapshotsSent; }
		NetHost& host() { return m_host; }

	private:
		struct Client
		{
			uint32_t peer = 0;
			std::string name;
			Actor* character = nullptr;
			uint32_t lastInputStep = 0;       // the newest of its inputs taken
			uint32_t ackSnapshot = 0;         // the newest snapshot it says it has
			PlayerInput input;
			uint32_t pendingPresses = 0;      // presses not used in a step yet
			std::map<uintptr_t, uint32_t> namedAt;   // the snapshot in which each actor's name went to it
		};

		void receive(Client& client, const std::vector<uint8_t>& data);
		Client* find(uint32_t peer);

		TopDownGame& m_game;
		Hooks m_hooks;
		std::vector<uint8_t> m_gameInfo;
		uint32_t m_stepHz, m_snapshotEvery;
		NetHost m_host;
		std::vector<Client> m_clients;
		uint32_t m_snapshotsSent = 0;
	};
}
