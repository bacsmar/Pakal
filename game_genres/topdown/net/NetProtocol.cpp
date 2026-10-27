///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: NetProtocol.cpp
// Purpose: What a TopDown server and its clients say to each other - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/net/NetProtocol.h"

#include <algorithm>
#include <cmath>

namespace Pakal::TopDown::Net
{
	namespace
	{
		const float kMeters = 0.01f;   // positions to the centimeter

		enum Flags : uint8_t
		{
			Named = 1, Dead = 2, Attacking = 4, Hurt = 8, Special = 16, HasHealth = 32, HasAction = 64,
		};

		float wrap_angle(float a)
		{
			const float pi = 3.14159265f;
			while (a > pi) a -= 2.0f * pi;
			while (a < -pi) a += 2.0f * pi;
			return a;
		}
	}

	void write_input(ByteWriter& out, const PlayerInput& input)
	{
		// the move to 1/127, the aim to the centimeter; the buttons as bits
		out.u8(static_cast<uint8_t>(static_cast<int8_t>(std::lround(std::clamp(input.move.x, -1.0f, 1.0f) * 127.0f))));
		out.u8(static_cast<uint8_t>(static_cast<int8_t>(std::lround(std::clamp(input.move.y, -1.0f, 1.0f) * 127.0f))));
		uint8_t flags = (input.hasAimPoint ? 1 : 0) | (input.fire ? 2 : 0) | (input.aimDirection.LengthSquared() > 0.0f ? 4 : 0);
		out.u8(flags);
		if (input.hasAimPoint)
		{
			out.quantized(input.aimPoint.x, kMeters);
			out.quantized(input.aimPoint.y, kMeters);
		}
		if (flags & 4) out.angle(std::atan2(input.aimDirection.y, input.aimDirection.x));
		out.varuint(input.held);
		out.varuint(input.pressed);
	}

	PlayerInput read_input(ByteReader& in)
	{
		PlayerInput input;
		input.move.x = static_cast<float>(static_cast<int8_t>(in.u8())) / 127.0f;
		input.move.y = static_cast<float>(static_cast<int8_t>(in.u8())) / 127.0f;
		const uint8_t flags = in.u8();
		input.hasAimPoint = (flags & 1) != 0;
		input.fire = (flags & 2) != 0;
		if (input.hasAimPoint)
		{
			input.aimPoint.x = in.quantized(kMeters);
			input.aimPoint.y = in.quantized(kMeters);
		}
		if (flags & 4)
		{
			const float a = in.angle();
			input.aimDirection = b2Vec2(std::cos(a), std::sin(a));
		}
		input.held = static_cast<uint32_t>(in.varuint());
		input.pressed = static_cast<uint32_t>(in.varuint());
		return input;
	}

	ActorState state_of(const TopDownGame& game, const Actor& actor)
	{
		ActorState state;
		state.id = actor.id;
		state.kind = actor.kind;
		state.faction = game.faction_of(actor);
		state.name = actor.name;
		state.position = actor.position;
		state.yaw = actor.yaw;
		if (const HealthComponent* health = actor.get<HealthComponent>())
		{
			state.health = std::clamp(health->current() / std::max(0.001f, health->maximum()), 0.0f, 1.0f);
			state.dead = health->is_dead();
		}
		if (const CharacterViewSource* source = actor.get<CharacterViewSource>())
		{
			const CharacterViewState view = source->view_state();
			state.dead = state.dead || view.dead;
			state.attacking = view.attacking;
			state.hurt = view.hurt;
			state.special = view.special;
			state.hurtClips = static_cast<uint8_t>(view.hurtClips & 0xff);
			state.facing = view.facing;
			if (view.action)
			{
				state.action = view.action->name;
				state.actionSerial = static_cast<uint8_t>(view.action->serial & 0xff);
				state.actionSeconds = view.action->seconds;
			}
		}
		return state;
	}

	void write_snapshot(ByteWriter& out, const Snapshot& snapshot, const std::function<bool(uintptr_t)>& named)
	{
		out.u8(static_cast<uint8_t>(Message::Snapshot));
		out.u32(snapshot.step);
		out.u32(snapshot.inputAck);
		out.varuint(snapshot.actors.size());
		for (const auto& a : snapshot.actors)
		{
			const bool withName = !named || !named(a.id);
			uint8_t flags = (withName ? Named : 0) | (a.dead ? Dead : 0) | (a.attacking ? Attacking : 0) | (a.hurt ? Hurt : 0)
				| (a.special ? Special : 0) | (a.health >= 0.0f ? HasHealth : 0) | (!a.action.empty() ? HasAction : 0);
			out.varuint(a.id);
			out.u8(flags);
			if (withName)
			{
				out.varint(a.kind);
				out.varint(a.faction);
				out.string(a.name);
			}
			out.quantized(a.position.x, kMeters);
			out.quantized(a.position.y, kMeters);
			out.angle(a.yaw);
			if (flags & HasHealth) out.u8(static_cast<uint8_t>(std::lround(a.health * 255.0f)));
			if (flags & HasAction)
			{
				out.string(a.action);
				out.u8(a.actionSerial);
				out.u16(static_cast<uint16_t>(std::clamp(a.actionSeconds * 100.0f, 0.0f, 65535.0f)));
			}
			out.u8(a.hurtClips);
			out.angle(std::atan2(a.facing.y, a.facing.x));
		}
		out.bytes(snapshot.own);
	}

	bool read_snapshot(ByteReader& in, Snapshot& snapshot, std::map<uintptr_t, ActorState>& known)
	{
		if (static_cast<Message>(in.u8()) != Message::Snapshot) return false;
		snapshot.step = in.u32();
		snapshot.inputAck = in.u32();
		const uint64_t count = in.varuint();
		snapshot.actors.clear();
		for (uint64_t i = 0; i < count && in.ok(); ++i)
		{
			ActorState a;
			a.id = static_cast<uintptr_t>(in.varuint());
			const uint8_t flags = in.u8();
			if (flags & Named)
			{
				a.kind = static_cast<int>(in.varint());
				a.faction = static_cast<int>(in.varint());
				a.name = in.string();
				ActorState& entry = known[a.id];
				entry.kind = a.kind;
				entry.faction = a.faction;
				entry.name = a.name;
			}
			else if (auto found = known.find(a.id); found != known.end())
			{
				a.kind = found->second.kind;
				a.faction = found->second.faction;
				a.name = found->second.name;
			}
			a.position.x = in.quantized(kMeters);
			a.position.y = in.quantized(kMeters);
			a.yaw = in.angle();
			a.dead = (flags & Dead) != 0;
			a.attacking = (flags & Attacking) != 0;
			a.hurt = (flags & Hurt) != 0;
			a.special = (flags & Special) != 0;
			if (flags & HasHealth) a.health = static_cast<float>(in.u8()) / 255.0f;
			if (flags & HasAction)
			{
				a.action = in.string();
				a.actionSerial = in.u8();
				a.actionSeconds = static_cast<float>(in.u16()) / 100.0f;
			}
			a.hurtClips = in.u8();
			const float facing = in.angle();
			a.facing = b2Vec2(std::cos(facing), std::sin(facing));
			snapshot.actors.push_back(std::move(a));
		}
		snapshot.own = in.bytes();
		return in.ok();
	}

	// SnapshotBuffer -------------------------------------------------------------------------------

	void SnapshotBuffer::add(Snapshot snapshot)
	{
		// late or repeated ones are of no use; the rest in step order
		if (!m_snapshots.empty() && snapshot.step <= m_snapshots.back().step) return;
		m_snapshots.push_back(std::move(snapshot));
		while (m_snapshots.size() > 32) m_snapshots.pop_front();
	}

	void SnapshotBuffer::advance(float steps)
	{
		if (m_snapshots.empty()) return;
		const double goal = static_cast<double>(m_snapshots.back().step) - m_delay;
		if (m_render < 0.0 || std::fabs(goal - m_render) > 30.0)
		{
			m_render = goal;   // the start, or too far off (a stall): jump
			return;
		}
		// a little faster or slower to stay at the delay behind the latest snapshot
		const double drift = goal - (m_render + steps);
		m_render += steps + std::clamp(drift * 0.1, -0.5 * steps, 0.5 * steps);
	}

	bool SnapshotBuffer::sample(std::vector<ActorState>& actors) const
	{
		actors.clear();
		if (m_snapshots.empty()) return false;
		// the two snapshots around the drawing time (before the first one: the first; after the last: the last)
		const Snapshot* from = &m_snapshots.front();
		const Snapshot* to = from;
		for (size_t i = 0; i < m_snapshots.size(); ++i)
		{
			if (static_cast<double>(m_snapshots[i].step) <= m_render) from = &m_snapshots[i];
			if (static_cast<double>(m_snapshots[i].step) >= m_render)
			{
				to = &m_snapshots[i];
				break;
			}
			to = &m_snapshots[i];
		}
		const double span = static_cast<double>(to->step) - static_cast<double>(from->step);
		const float t = span > 0.0 ? static_cast<float>(std::clamp((m_render - from->step) / span, 0.0, 1.0)) : 1.0f;
		std::map<uintptr_t, const ActorState*> next;
		for (const auto& a : to->actors) next[a.id] = &a;
		for (const auto& a : from->actors)
		{
			auto later = next.find(a.id);
			if (later == next.end())
			{
				actors.push_back(a);   // gone in the later one: drawn until then
				continue;
			}
			// what it shows from the nearer snapshot; where it is and where it faces, in between (a jump of meters, a
			// respawn, is not smoothed)
			const ActorState& b = *later->second;
			ActorState drawn = t < 0.5f ? a : b;
			if ((b.position - a.position).LengthSquared() < 16.0f) drawn.position = a.position + t * (b.position - a.position);
			drawn.yaw = a.yaw + t * wrap_angle(b.yaw - a.yaw);
			actors.push_back(std::move(drawn));
		}
		// the ones that came in the later snapshot appear once it is reached
		if (t >= 1.0f)
		{
			for (const auto& a : to->actors)
			{
				if (std::none_of(actors.begin(), actors.end(), [&](const ActorState& s) { return s.id == a.id; })) actors.push_back(a);
			}
		}
		return true;
	}
}
