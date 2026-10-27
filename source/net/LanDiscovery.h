///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LanDiscovery.h
// Purpose: Finding games on the local network without typing an address. A server keeps a LanBeacon
//          that answers the question "who plays <game> here?", sent to the whole network (UDP
//          broadcast) by a LanBrowser, with the port its game listens on and a line about it (its
//          name, its players). Neither blocks: both are polled.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Pakal
{
	// The port the questions go to (the games' own ports are the answers)
	constexpr uint16_t kLanDiscoveryPort = 27961;

	class LanBeacon
	{
	public:
		LanBeacon() = default;
		~LanBeacon() { close(); }
		LanBeacon(const LanBeacon&) = delete;
		LanBeacon& operator=(const LanBeacon&) = delete;

		// Answers for 'game' (its name: other games' questions go unanswered) that it plays on 'gamePort'
		bool open(const std::string& game, uint16_t gamePort, uint16_t port = kLanDiscoveryPort);
		void close();
		// The line the answers carry ("ana's dungeon, 2 players")
		void set_info(const std::string& info) { m_info = info; }
		// Answers the questions that came
		void poll();

	private:
		intptr_t m_socket = -1;
		std::string m_game, m_info;
		uint16_t m_gamePort = 0;
	};

	class LanBrowser
	{
	public:
		struct Found
		{
			std::string host;    // its address ("192.168.1.20")
			uint16_t port = 0;   // its game's port
			std::string info;
			double seen = 0.0;   // when it last answered (the browser's seconds)
		};

		LanBrowser() = default;
		~LanBrowser() { close(); }
		LanBrowser(const LanBrowser&) = delete;
		LanBrowser& operator=(const LanBrowser&) = delete;

		bool open(const std::string& game, uint16_t port = kLanDiscoveryPort);
		void close();
		// Asks the network again (every second or so); 'now' in seconds, to forget who stopped answering
		void search(double now);
		// The answers that came; a game that has not answered for 'forgetAfter' seconds goes
		void poll(double now, double forgetAfter = 4.0);
		const std::vector<Found>& found() const { return m_found; }

	private:
		intptr_t m_socket = -1;
		std::string m_game;
		uint16_t m_port = kLanDiscoveryPort;
		std::vector<Found> m_found;
	};
}
