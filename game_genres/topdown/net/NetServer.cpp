///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: NetServer.cpp
// Purpose: A TopDown game played over the network, the server's side - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/net/NetServer.h"

#include "LogMgr.h"

#include <algorithm>

namespace Pakal::TopDown::Net
{
	NetServer::NetServer(TopDownGame& game, Hooks hooks, std::vector<uint8_t> gameInfo, uint32_t stepHz, uint32_t snapshotEvery) :
		m_game(game),
		m_hooks(std::move(hooks)),
		m_gameInfo(std::move(gameInfo)),
		m_stepHz(stepHz),
		m_snapshotEvery(std::max(1u, snapshotEvery))
	{
	}

	bool NetServer::listen(uint16_t port, size_t maxClients)
	{
		return m_host.listen(port, maxClients, 2);
	}

	NetServer::Client* NetServer::find(uint32_t peer)
	{
		for (auto& client : m_clients)
		{
			if (client.peer == peer) return &client;
		}
		return nullptr;
	}

	void NetServer::poll()
	{
		for (auto& event : m_host.poll(0))
		{
			switch (event.type)
			{
			case NetHost::Event::Type::Connected:
				m_clients.push_back(Client{ event.peer });   // a player once it says hello
				break;
			case NetHost::Event::Type::Disconnected:
			{
				auto found = std::find_if(m_clients.begin(), m_clients.end(), [&](const Client& c) { return c.peer == event.peer; });
				if (found == m_clients.end()) break;
				LOG_INFO("[NetServer] '%s' left", found->name.c_str());
				if (found->character && m_hooks.leave) m_hooks.leave(event.peer, *found->character);
				m_clients.erase(found);
				break;
			}
			case NetHost::Event::Type::Received:
				if (Client* client = find(event.peer)) receive(*client, event.data);
				break;
			}
		}
	}

	void NetServer::receive(Client& client, const std::vector<uint8_t>& data)
	{
		ByteReader in(data);
		const Message type = static_cast<Message>(in.u8());
		if (type == Message::Hello && !client.character)
		{
			const uint32_t version = in.u32();
			client.name = in.string();
			ByteWriter reply;
			Actor* character = version == kProtocolVersion && in.ok() && m_hooks.join ? m_hooks.join(client.peer, client.name) : nullptr;
			if (!character)
			{
				reply.u8(static_cast<uint8_t>(Message::Refused));
				reply.string(version != kProtocolVersion ? "another version of the protocol" : "the game is full");
				m_host.send(client.peer, kReliableChannel, reply.data(), true);
				m_host.disconnect(client.peer);
				return;
			}
			client.character = character;
			uint32_t playerIndex = 0;
			for (size_t i = 0; i < m_game.players().size(); ++i)
			{
				if (m_game.players()[i] == character) playerIndex = static_cast<uint32_t>(i);
			}
			reply.u8(static_cast<uint8_t>(Message::Welcome));
			reply.u32(playerIndex);
			reply.varuint(character->id);
			reply.u32(m_stepHz);
			reply.u32(m_snapshotEvery);
			reply.bytes(m_gameInfo);
			m_host.send(client.peer, kReliableChannel, reply.data(), true);
			LOG_INFO("[NetServer] '%s' joined as player %u", client.name.c_str(), playerIndex);
			return;
		}
		if (type == Message::Command && client.character)
		{
			if (m_hooks.command) m_hooks.command(*client.character, in);
			return;
		}
		if (type == Message::Input && client.character)
		{
			// its step, the last snapshot it has, then its inputs, newest first: the ones newer than what was taken count
			const uint32_t step = in.u32();
			client.ackSnapshot = std::max(client.ackSnapshot, in.u32());
			const uint8_t count = in.u8();
			uint32_t presses = 0;
			PlayerInput newest;
			for (uint8_t i = 0; i < count && in.ok(); ++i)
			{
				const PlayerInput input = read_input(in);
				const uint32_t inputStep = step - i;
				if (i == 0) newest = input;
				if (inputStep > client.lastInputStep) presses |= input.pressed;
			}
			if (!in.ok() || step <= client.lastInputStep) return;   // broken, or old
			client.lastInputStep = step;
			client.input = newest;
			client.pendingPresses |= presses;
		}
	}

	void NetServer::fill_inputs(std::vector<PlayerInput>& inputs)
	{
		const auto& players = m_game.players();
		if (inputs.size() < players.size()) inputs.resize(players.size());
		for (auto& client : m_clients)
		{
			for (size_t i = 0; i < players.size(); ++i)
			{
				if (!client.character || players[i] != client.character) continue;
				inputs[i] = client.input;
				inputs[i].pressed = client.pendingPresses;   // a press is used once
				client.pendingPresses = 0;
			}
		}
	}

	void NetServer::end_step(uint32_t step)
	{
		if (step % m_snapshotEvery != 0 || m_clients.empty()) return;
		Snapshot snapshot;
		snapshot.step = step;
		for (const auto& actor : m_game.actors())
		{
			if (!actor->removed) snapshot.actors.push_back(state_of(m_game, *actor));
		}
		for (auto& client : m_clients)
		{
			if (!client.character) continue;
			snapshot.inputAck = client.lastInputStep;
			snapshot.own.clear();
			if (m_hooks.own)
			{
				ByteWriter own;
				m_hooks.own(own, *client.character);
				snapshot.own = std::move(own.data());
			}
			// a name goes until the client has a snapshot that carried it
			ByteWriter out;
			write_snapshot(out, snapshot, [&](uintptr_t id)
			{
				auto named = client.namedAt.find(id);
				if (named != client.namedAt.end() && client.ackSnapshot >= named->second) return true;
				if (named == client.namedAt.end()) client.namedAt[id] = step;
				return false;
			});
			m_host.send(client.peer, kUnreliableChannel, out.data(), false);
			++m_snapshotsSent;
		}
		m_host.flush();
	}
}
