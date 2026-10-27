///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: NetHost.cpp
// Purpose: One end of a network connection over UDP (ENet) - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "net/NetHost.h"

#include <enet/enet.h>

#include <algorithm>
#include <mutex>

namespace Pakal
{
	namespace
	{
		// ENet starts once per process, and ends with the last host
		std::mutex g_enetMutex;
		int g_enetUsers = 0;

		bool enet_acquire()
		{
			std::lock_guard<std::mutex> lock(g_enetMutex);
			if (g_enetUsers == 0 && enet_initialize() != 0) return false;
			++g_enetUsers;
			return true;
		}

		void enet_release()
		{
			std::lock_guard<std::mutex> lock(g_enetMutex);
			if (g_enetUsers > 0 && --g_enetUsers == 0) enet_deinitialize();
		}

		const uint32_t kServerPeer = 0;   // a client's one peer
	}

	bool net_acquire() { return enet_acquire(); }
	void net_release() { enet_release(); }

	NetHost::NetHost() = default;

	NetHost::~NetHost()
	{
		close();
	}

	bool NetHost::listen(uint16_t port, size_t maxPeers, uint8_t channels)
	{
		close();
		if (!enet_acquire()) return false;
		ENetAddress address;
		address.host = ENET_HOST_ANY;
		address.port = port;
		m_host = enet_host_create(&address, maxPeers, channels, 0, 0);
		if (!m_host)
		{
			enet_release();
			return false;
		}
		m_client = false;
		return true;
	}

	bool NetHost::connect(const std::string& host, uint16_t port, uint8_t channels)
	{
		close();
		if (!enet_acquire()) return false;
		m_host = enet_host_create(nullptr, 1, channels, 0, 0);
		if (!m_host)
		{
			enet_release();
			return false;
		}
		ENetAddress address;
		if (enet_address_set_host(&address, host.c_str()) != 0)
		{
			close();
			return false;
		}
		address.port = port;
		ENetPeer* peer = enet_host_connect(m_host, &address, channels, 0);
		if (!peer)
		{
			close();
			return false;
		}
		m_client = true;
		m_peers.push_back({ kServerPeer, peer });
		return true;
	}

	void NetHost::close()
	{
		if (!m_host) return;
		// a polite goodbye to whoever is there, sent now
		for (const auto& peer : m_peers) enet_peer_disconnect_now(peer.peer, 0);
		enet_host_flush(m_host);
		enet_host_destroy(m_host);
		m_host = nullptr;
		m_peers.clear();
		m_connected = false;
		enet_release();
	}

	uint16_t NetHost::port() const
	{
		return m_host ? m_host->address.port : 0;
	}

	ENetPeer* NetHost::find(uint32_t peer) const
	{
		for (const auto& p : m_peers)
		{
			if (p.id == peer) return p.peer;
		}
		return nullptr;
	}

	void NetHost::send(uint32_t peer, uint8_t channel, const std::vector<uint8_t>& data, bool reliable)
	{
		ENetPeer* target = find(peer);
		if (!target || !m_host) return;
		ENetPacket* packet = enet_packet_create(data.data(), data.size(), reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
		if (enet_peer_send(target, channel, packet) != 0) enet_packet_destroy(packet);
	}

	void NetHost::broadcast(uint8_t channel, const std::vector<uint8_t>& data, bool reliable)
	{
		if (!m_host) return;
		ENetPacket* packet = enet_packet_create(data.data(), data.size(), reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
		enet_host_broadcast(m_host, channel, packet);
	}

	void NetHost::disconnect(uint32_t peer)
	{
		if (ENetPeer* target = find(peer)) enet_peer_disconnect(target, 0);
	}

	NetHost::PeerStats NetHost::stats(uint32_t peer) const
	{
		PeerStats stats;
		if (const ENetPeer* target = find(peer))
		{
			stats.roundTripMs = target->roundTripTime;
			stats.packetLoss = static_cast<float>(target->packetLoss) / static_cast<float>(ENET_PEER_PACKET_LOSS_SCALE);
		}
		return stats;
	}

	void NetHost::flush()
	{
		if (m_host) enet_host_flush(m_host);
	}

	std::vector<NetHost::Event> NetHost::poll(uint32_t timeoutMs)
	{
		std::vector<Event> events;
		if (!m_host) return events;
		ENetEvent event;
		uint32_t wait = timeoutMs;
		while (enet_host_service(m_host, &event, wait) > 0)
		{
			wait = 0;   // only the first one waits
			switch (event.type)
			{
			case ENET_EVENT_TYPE_CONNECT:
			{
				uint32_t id = kServerPeer;
				if (m_client)
				{
					m_connected = true;
				}
				else
				{
					id = m_nextId++;
					m_peers.push_back({ id, event.peer });
				}
				event.peer->data = reinterpret_cast<void*>(static_cast<uintptr_t>(id));
				events.push_back({ Event::Type::Connected, id, 0, {} });
				break;
			}
			case ENET_EVENT_TYPE_DISCONNECT:
			{
				const uint32_t id = m_client ? kServerPeer : static_cast<uint32_t>(reinterpret_cast<uintptr_t>(event.peer->data));
				if (m_client) m_connected = false;
				else m_peers.erase(std::remove_if(m_peers.begin(), m_peers.end(), [&](const Peer& p) { return p.peer == event.peer; }), m_peers.end());
				events.push_back({ Event::Type::Disconnected, id, 0, {} });
				break;
			}
			case ENET_EVENT_TYPE_RECEIVE:
			{
				const uint32_t id = m_client ? kServerPeer : static_cast<uint32_t>(reinterpret_cast<uintptr_t>(event.peer->data));
				Event received{ Event::Type::Received, id, event.channelID, {} };
				received.data.assign(event.packet->data, event.packet->data + event.packet->dataLength);
				enet_packet_destroy(event.packet);
				events.push_back(std::move(received));
				break;
			}
			default:
				break;
			}
		}
		return events;
	}
}
