///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: NetProtocol.h
// Purpose: What a TopDown server and its clients say to each other. The server runs the simulation; each
//          client sends the input of its player every step (the last few again, so a lost packet costs
//          nothing) and receives snapshots: every actor as it is (position to the centimeter, facing,
//          health, what its model shows). An actor's name, kind and faction go only until the client
//          says it has a snapshot with them. A client draws the world a little behind the latest
//          snapshot, between two of them (SnapshotBuffer), so it moves smoothly.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "net/ByteStream.h"
#include "topdown/TopDownComponents.h"
#include "topdown/TopDownGame.h"

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace Pakal::TopDown::Net
{
	constexpr uint32_t kProtocolVersion = 2;
	constexpr uint8_t kReliableChannel = 0;     // hello, welcome
	constexpr uint8_t kUnreliableChannel = 1;   // inputs, snapshots: the latest wins

	enum class Message : uint8_t
	{
		Hello = 1,   // client: protocol version, a name
		Welcome,     // server: the client's player and actor, the step rate, the game's own data (its seed...)
		Refused,     // server: why it cannot join
		Input,       // client: its step, the last snapshot it has, its latest inputs (newest first)
		Snapshot,    // server: its step, the client's last input it used, the actors, the player's own data
		Command,     // client: something its player does that is not a step's input (the game's bytes: wear an item)
	};

	struct Welcome
	{
		uint32_t playerIndex = 0;
		uintptr_t actor = 0;          // the client's own character
		uint32_t stepHz = 60;         // simulation steps per second
		uint32_t snapshotEvery = 3;   // steps between snapshots
		std::vector<uint8_t> game;    // what the game needs to build its world as the server did (a seed, a level)
	};

	// One actor in a snapshot
	struct ActorState
	{
		uintptr_t id = 0;
		int kind = 0;
		int faction = 0;
		std::string name;
		b2Vec2 position = b2Vec2(0.0f, 0.0f);
		float yaw = 0.0f;
		float health = -1.0f;         // share of its maximum (0..1); negative: it has none
		bool dead = false, attacking = false, hurt = false, special = false;
		std::string action;           // the action its model plays ("" for none)
		uint8_t actionSerial = 0;     // changes when an action starts again
		float actionSeconds = 0.0f;
		uint8_t hurtClips = 0;
		b2Vec2 facing = b2Vec2(0.0f, 1.0f);
	};

	struct Snapshot
	{
		uint32_t step = 0;
		uint32_t inputAck = 0;        // the client's last input step the server had used
		std::vector<ActorState> actors;
		std::vector<uint8_t> own;     // the receiving player's own data (the game's: mana, gold, cooldowns)
	};

	void write_input(ByteWriter& out, const PlayerInput& input);
	PlayerInput read_input(ByteReader& in);

	// An actor as the server sees it (its health, what its model shows)
	ActorState state_of(const TopDownGame& game, const Actor& actor);
	// 'named': whether the client already knows the actor's name, kind and faction (else they go too)
	void write_snapshot(ByteWriter& out, const Snapshot& snapshot, const std::function<bool(uintptr_t)>& named);
	// 'known' keeps the names, kinds and factions of the actors seen so far (by id)
	bool read_snapshot(ByteReader& in, Snapshot& snapshot, std::map<uintptr_t, ActorState>& known);

	// The snapshots a client received, and the world between two of them at a time a little behind the latest
	class SnapshotBuffer
	{
	public:
		// 'delaySteps': how far behind the latest snapshot the world is drawn (two snapshots: one may be lost)
		explicit SnapshotBuffer(float delaySteps = 6.0f) : m_delay(delaySteps) {}

		void add(Snapshot snapshot);
		// The drawing clock goes on 'steps' (seconds times the step rate), and is kept near the latest snapshot
		void advance(float steps);
		// The actors at the drawing time: positions and facing in between, the rest as the earlier snapshot has them.
		// False until there are snapshots.
		bool sample(std::vector<ActorState>& actors) const;
		const Snapshot* latest() const { return m_snapshots.empty() ? nullptr : &m_snapshots.back(); }
		double render_step() const { return m_render; }

	private:
		std::deque<Snapshot> m_snapshots;
		float m_delay;
		double m_render = -1.0;
	};
}
