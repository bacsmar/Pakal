///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: NetClient.cpp
// Purpose: A TopDown game played over the network, a client's side - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/net/NetClient.h"

#include "LogMgr.h"

namespace Pakal::TopDown::Net
{
	namespace
	{
		const size_t kRedundantInputs = 4;   // each input message carries the last four
	}

	bool NetClient::connect(const std::string& host, uint16_t port, const std::string& name)
	{
		m_name = name;
		m_helloSent = m_welcomed = false;
		m_refused.clear();
		return m_host.connect(host, port, 2);
	}

	void NetClient::poll(float seconds)
	{
		for (auto& event : m_host.poll(0))
		{
			if (event.type == NetHost::Event::Type::Disconnected)
			{
				LOG_INFO("[NetClient] the server is gone");
				m_welcomed = false;
				continue;
			}
			if (event.type != NetHost::Event::Type::Received) continue;
			ByteReader in(event.data);
			const Message type = static_cast<Message>(event.data.empty() ? 0 : event.data[0]);
			if (type == Message::Welcome)
			{
				in.u8();
				m_welcome.playerIndex = in.u32();
				m_welcome.actor = static_cast<uintptr_t>(in.varuint());
				m_welcome.stepHz = in.u32();
				m_welcome.snapshotEvery = in.u32();
				m_welcome.game = in.bytes();
				m_welcomed = in.ok();
				// two snapshots behind the latest: one of them may be lost on the way
				m_buffer = SnapshotBuffer(2.0f * static_cast<float>(m_welcome.snapshotEvery));
			}
			else if (type == Message::Refused)
			{
				in.u8();
				m_refused = in.string();
				LOG_WARNING("[NetClient] refused: %s", m_refused.c_str());
			}
			else if (type == Message::Snapshot && m_welcomed)
			{
				Snapshot snapshot;
				if (read_snapshot(in, snapshot, m_known))
				{
					m_own = snapshot.own;
					++m_received;
					m_buffer.add(std::move(snapshot));
				}
			}
		}
		// hello once connected
		if (m_host.connected() && !m_helloSent)
		{
			ByteWriter hello;
			hello.u8(static_cast<uint8_t>(Message::Hello));
			hello.u32(kProtocolVersion);
			hello.string(m_name);
			m_host.send(0, kReliableChannel, hello.data(), true);
			m_host.flush();
			m_helloSent = true;
		}
		if (m_welcomed)
		{
			m_buffer.advance(seconds * static_cast<float>(m_welcome.stepHz));
			m_buffer.sample(m_drawn);
		}
	}

	void NetClient::send_command(const std::vector<uint8_t>& data)
	{
		if (!m_welcomed) return;
		ByteWriter out;
		out.u8(static_cast<uint8_t>(Message::Command));
		for (uint8_t b : data) out.u8(b);
		m_host.send(0, kReliableChannel, out.data(), true);
		m_host.flush();
	}

	void NetClient::send_input(const PlayerInput& input)
	{
		if (!m_welcomed) return;
		++m_step;
		m_recent.insert(m_recent.begin(), input);
		if (m_recent.size() > kRedundantInputs) m_recent.pop_back();
		ByteWriter out;
		out.u8(static_cast<uint8_t>(Message::Input));
		out.u32(m_step);
		const Snapshot* latest = m_buffer.latest();
		out.u32(latest ? latest->step : 0);
		out.u8(static_cast<uint8_t>(m_recent.size()));
		for (const auto& recent : m_recent) write_input(out, recent);
		m_host.send(0, kUnreliableChannel, out.data(), false);
		m_host.flush();
	}
}
