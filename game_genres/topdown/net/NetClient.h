///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: NetClient.h
// Purpose: A TopDown game played over the network, a client's side: it joins a server, sends its player's
//          input every step and keeps the snapshots it receives, to draw the world between two of them.
//          It runs no simulation of its own; ReplicaGame holds the actors it draws.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "net/NetHost.h"
#include "topdown/net/NetProtocol.h"

#include <map>
#include <string>
#include <vector>

namespace Pakal::TopDown::Net
{
	class NetClient
	{
	public:
		bool connect(const std::string& host, uint16_t port, const std::string& name);
		void close() { m_host.close(); }

		// What came from the server; the drawing clock goes on 'seconds'
		void poll(float seconds);
		// Each step: this player's input (sent with the last few, so one lost packet costs nothing)
		void send_input(const PlayerInput& input);
		// Something the player does besides its input (the game's bytes: NetServer::Hooks::command), sent reliably
		void send_command(const std::vector<uint8_t>& data);

		bool connected() const { return m_host.connected(); }
		bool welcomed() const { return m_welcomed; }
		// Why the server would not let it in ("" if it did)
		const std::string& refused() const { return m_refused; }
		const Welcome& welcome() const { return m_welcome; }
		// The world as it is drawn now (empty until a snapshot came)
		const std::vector<ActorState>& actors() const { return m_drawn; }
		// The latest snapshot's data for this player
		const std::vector<uint8_t>& own() const { return m_own; }
		uint32_t snapshots() const { return m_received; }
		NetHost::PeerStats stats() const { return m_host.stats(0); }

	private:
		NetHost m_host;
		std::string m_name;
		bool m_helloSent = false;
		bool m_welcomed = false;
		std::string m_refused;
		Welcome m_welcome;
		SnapshotBuffer m_buffer;
		std::map<uintptr_t, ActorState> m_known;
		std::vector<ActorState> m_drawn;
		std::vector<uint8_t> m_own;
		uint32_t m_received = 0;
		uint32_t m_step = 0;
		std::vector<PlayerInput> m_recent;   // newest first
	};
}
