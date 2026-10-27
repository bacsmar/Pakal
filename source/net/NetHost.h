///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: NetHost.h
// Purpose: One end of a network connection over UDP (ENet): a server that listens for peers, or a client
//          with one peer, the server. Messages are bytes on a channel, sent reliably (they arrive, in
//          order) or not (the latest wins: a snapshot, an input). poll() hands over what happened since
//          the last call (peers that came and went, messages). Peers are named by a number this host
//          gives them. Not thread-safe: one thread uses a host.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct _ENetHost;
struct _ENetPeer;

namespace Pakal
{
	// ENet starts with the first user in the process and ends with the last (NetHost and LanDiscovery call them)
	bool net_acquire();
	void net_release();

	class NetHost
	{
	public:
		struct Event
		{
			enum class Type { Connected, Disconnected, Received };
			Type type = Type::Received;
			uint32_t peer = 0;
			uint8_t channel = 0;
			std::vector<uint8_t> data;
		};

		struct PeerStats
		{
			uint32_t roundTripMs = 0;
			float packetLoss = 0.0f;   // 0..1
		};

		NetHost();
		~NetHost();
		NetHost(const NetHost&) = delete;
		NetHost& operator=(const NetHost&) = delete;

		// A server on 'port' (0: any free port; port() says which) for up to 'maxPeers'
		bool listen(uint16_t port, size_t maxPeers, uint8_t channels = 2);
		// A client: starts connecting (poll() reports Connected, or Disconnected if it fails)
		bool connect(const std::string& host, uint16_t port, uint8_t channels = 2);
		void close();

		bool is_open() const { return m_host != nullptr; }
		uint16_t port() const;
		// A client: connected to the server (its peer is 0)
		bool connected() const { return m_connected; }

		void send(uint32_t peer, uint8_t channel, const std::vector<uint8_t>& data, bool reliable);
		void broadcast(uint8_t channel, const std::vector<uint8_t>& data, bool reliable);
		void disconnect(uint32_t peer);
		PeerStats stats(uint32_t peer) const;
		size_t peer_count() const { return m_peers.size(); }

		// What happened (waits up to 'timeoutMs' for the first event); also sends what was queued
		std::vector<Event> poll(uint32_t timeoutMs = 0);
		// Sends what was queued now, without waiting for poll
		void flush();

	private:
		_ENetPeer* find(uint32_t peer) const;

		_ENetHost* m_host = nullptr;
		struct Peer { uint32_t id; _ENetPeer* peer; };
		std::vector<Peer> m_peers;
		uint32_t m_nextId = 1;
		bool m_client = false;
		bool m_connected = false;
	};
}
