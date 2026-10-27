///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LanDiscovery.cpp
// Purpose: Finding games on the local network - implementation. A question is "PKLQ", the game's
//          name; an answer is "PKLA", the game's name, its port and its line.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "net/LanDiscovery.h"
#include "net/ByteStream.h"
#include "net/NetHost.h"

#include <enet/enet.h>

#include <algorithm>

namespace Pakal
{
	namespace
	{
		const uint32_t kQuestion = 0x514c4b50;   // "PKLQ"
		const uint32_t kAnswer = 0x414c4b50;     // "PKLA"

		// A datagram socket that does not block; 'bindPort' 0 takes any free port
		intptr_t open_socket(uint16_t bindPort, bool shared)
		{
			if (!net_acquire()) return -1;
			const ENetSocket socket = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
			if (socket == ENET_SOCKET_NULL)
			{
				net_release();
				return -1;
			}
			enet_socket_set_option(socket, ENET_SOCKOPT_NONBLOCK, 1);
			enet_socket_set_option(socket, ENET_SOCKOPT_BROADCAST, 1);
			if (shared) enet_socket_set_option(socket, ENET_SOCKOPT_REUSEADDR, 1);
			ENetAddress address;
			address.host = ENET_HOST_ANY;
			address.port = bindPort;
			if (enet_socket_bind(socket, &address) < 0)
			{
				enet_socket_destroy(socket);
				net_release();
				return -1;
			}
			return static_cast<intptr_t>(socket);
		}

		void close_socket(intptr_t& socket)
		{
			if (socket == -1) return;
			enet_socket_destroy(static_cast<ENetSocket>(socket));
			socket = -1;
			net_release();
		}

		void send_to(intptr_t socket, const ENetAddress& to, const std::vector<uint8_t>& data)
		{
			ENetBuffer buffer;
			buffer.data = const_cast<uint8_t*>(data.data());
			buffer.dataLength = data.size();
			enet_socket_send(static_cast<ENetSocket>(socket), &to, &buffer, 1);
		}

		// The next datagram (false: none waiting)
		bool receive(intptr_t socket, ENetAddress& from, std::vector<uint8_t>& data)
		{
			data.resize(512);
			ENetBuffer buffer;
			buffer.data = data.data();
			buffer.dataLength = data.size();
			const int length = enet_socket_receive(static_cast<ENetSocket>(socket), &from, &buffer, 1);
			if (length <= 0) return false;
			data.resize(static_cast<size_t>(length));
			return true;
		}
	}

	bool LanBeacon::open(const std::string& game, uint16_t gamePort, uint16_t port)
	{
		close();
		m_game = game;
		m_gamePort = gamePort;
		m_socket = open_socket(port, true);
		return m_socket != -1;
	}

	void LanBeacon::close()
	{
		close_socket(m_socket);
	}

	void LanBeacon::poll()
	{
		if (m_socket == -1) return;
		ENetAddress from;
		std::vector<uint8_t> data;
		while (receive(m_socket, from, data))
		{
			ByteReader in(data);
			if (in.u32() != kQuestion || in.string() != m_game || !in.ok()) continue;
			ByteWriter out;
			out.u32(kAnswer);
			out.string(m_game);
			out.u16(m_gamePort);
			out.string(m_info);
			send_to(m_socket, from, out.data());
		}
	}

	bool LanBrowser::open(const std::string& game, uint16_t port)
	{
		close();
		m_game = game;
		m_port = port;
		m_found.clear();
		m_socket = open_socket(0, false);
		return m_socket != -1;
	}

	void LanBrowser::close()
	{
		close_socket(m_socket);
	}

	void LanBrowser::search(double now)
	{
		(void)now;
		if (m_socket == -1) return;
		ByteWriter out;
		out.u32(kQuestion);
		out.string(m_game);
		ENetAddress everyone;
		everyone.host = ENET_HOST_BROADCAST;
		everyone.port = m_port;
		send_to(m_socket, everyone, out.data());
		// and this machine, where a broadcast does not always come back
		ENetAddress here;
		enet_address_set_host_ip(&here, "127.0.0.1");
		here.port = m_port;
		send_to(m_socket, here, out.data());
	}

	void LanBrowser::poll(double now, double forgetAfter)
	{
		if (m_socket == -1) return;
		ENetAddress from;
		std::vector<uint8_t> data;
		while (receive(m_socket, from, data))
		{
			ByteReader in(data);
			if (in.u32() != kAnswer || in.string() != m_game) continue;
			Found game;
			game.port = in.u16();
			game.info = in.string();
			if (!in.ok()) continue;
			char ip[64] = {};
			enet_address_get_host_ip(&from, ip, sizeof(ip));
			game.host = ip;
			game.seen = now;
			// one game answers twice on its own machine (to the broadcast and to 127.0.0.1): the same port and line
			auto known = std::find_if(m_found.begin(), m_found.end(), [&](const Found& f)
			{
				return f.port == game.port && (f.host == game.host || f.info == game.info);
			});
			if (known != m_found.end() && known->host != game.host) game.host = known->host;
			if (known != m_found.end()) *known = game;
			else m_found.push_back(game);
		}
		m_found.erase(std::remove_if(m_found.begin(), m_found.end(), [&](const Found& f) { return now - f.seen > forgetAfter; }), m_found.end());
	}
}
