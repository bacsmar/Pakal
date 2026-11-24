///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: QuestArbiter.h
// Purpose: The one owner of what a game's quests show (docs/QUESTS.md, section 8): the guiding marker, the
//          objective lines, the secondary marks of the tracked quest. Each system that wants something shown
//          states a claim under its own owner name; at the end of a step the arbiter resolves every claim
//          into one Presentation, a plain value: the best-ranked claim that shows anything (the tracked owner
//          first, then priority, then claim order) gives BOTH the marker and the lines, so the arrow and the
//          line the player reads never come from two quests. Every decision is logged (a "QUEST" line, a log
//          that tools read). What other threads read (the presentation, the state, the log, the game's own
//          part) lives in a shared block a tool keeps alive while it reads, whatever happens to the arbiter.
//          Genre-neutral: a target is opaque to it (a kind and an id the game resolves).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <nlohmann/json.hpp>

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace Pakal::Quests
{
	// What a marker points at: opaque to the core (the game resolves it: a level point, a prop, an actor)
	struct Target
	{
		std::string kind;          // the game's ("point", "prop", "actor"...)
		uint64_t id = 0;
		bool operator==(const Target& other) const { return kind == other.kind && id == other.id; }
		bool operator!=(const Target& other) const { return !(*this == other); }
	};

	// A line the player reads: a text (a key of the game's strings, or the source text) and its values
	struct ObjectiveLine
	{
		std::string text;
		nlohmann::json values = nlohmann::json::object();   // seconds left, a count and its goal, an index...
		bool operator==(const ObjectiveLine& other) const { return text == other.text && values == other.values; }
	};

	// What one owner asks to show. An owner is a system or a quest ("errand", "story", "outpost:a"...)
	struct Claim
	{
		int priority = 0;                        // higher wins (a game's kinds: defense 90, errand 50, story 40, guide 10)
		std::optional<Target> marker;            // its guiding marker
		std::vector<Target> secondary;           // shown while it is the tracked claim (else while it wins)
		std::vector<ObjectiveLine> objectives;   // its lines
		bool shows() const { return marker.has_value() || !objectives.empty(); }
		bool operator==(const Claim& other) const
		{
			return priority == other.priority && marker == other.marker && secondary == other.secondary && objectives == other.objectives;
		}
	};

	struct Marker
	{
		Target target;
		std::string owner;
		bool operator==(const Marker& other) const { return target == other.target && owner == other.owner; }
	};

	// What is shown, resolved from every claim: a plain value a presenter fills its UI from
	struct Presentation
	{
		uint32_t revision = 0;                   // bumps when anything in it changed
		std::string owner;                       // the claim shown: its marker and its lines ("" nothing shown)
		std::optional<Marker> marker;            // the winner's guiding marker (none when it has none)
		std::vector<Marker> secondary;           // the tracked claim's other marks (else the winner's)
		std::vector<ObjectiveLine> objectives;   // the winner's lines
		std::string tracked;                     // the owner the player follows ("" none)
		bool operator==(const Presentation& other) const
		{
			return owner == other.owner && marker == other.marker && secondary == other.secondary && objectives == other.objectives
				&& tracked == other.tracked;
		}
	};

	nlohmann::json to_json(const Target& target);
	nlohmann::json to_json(const Presentation& presentation);

	// What other threads read, kept alive by whoever holds it (a tool's call in flight), whatever happens to the arbiter
	struct Published
	{
		mutable std::mutex mutex;
		Presentation presentation;
		nlohmann::json state = nlohmann::json::object();
		nlohmann::json game = nlohmann::json::object();   // the game's own part (its quests, flags), set by publish_game
		std::deque<nlohmann::json> log;                    // the last decisions (bounded)
		uint64_t nextSeq = 1;
		static constexpr size_t kLogKept = 2000;
	};

	class Arbiter
	{
	public:
		// One line per decision ("QUEST ..."), for the game's log
		using LogLine = std::function<void(const std::string& line)>;

		// Sets or replaces an owner's claim (logged when it changed). Inside the simulation's step.
		void claim(const std::string& owner, const Claim& claim);
		// Drops an owner's claim (nothing when it has none)
		void release(const std::string& owner);
		bool has_claim(const std::string& owner) const { return m_claims.count(owner) > 0; }
		const Claim* claim_of(const std::string& owner) const;
		// The owner the player follows (its claim wins whatever its priority while it shows something); "" none
		void set_tracked(const std::string& owner);

		// At the step's start: the step its claims and releases are logged with (QA's log replays by step)
		void begin_step(uint64_t step) { m_step = step; }
		// At the step's end: the claims resolved into the presentation (its revision bumped only when it changed), then
		// published for other threads. Returns the core's own value (the simulation reads it; a presenter takes a copy)
		const Presentation& resolve(uint64_t step);
		const Presentation& presentation() const { return m_presentation; }

		// What other threads read (any thread): the last published presentation, the state, the decisions' log after a
		// sequence number (each entry {seq, step, event, owner, detail}); the shared block itself for a tool to hold
		Presentation published() const;
		nlohmann::json state() const;
		nlohmann::json log_since(uint64_t seq) const;
		std::shared_ptr<const Published> shared() const { return m_shared; }
		// The game's own part of the state (its quests, flags): a copy, set from the simulation's thread
		void publish_game(nlohmann::json game);

		void set_log_line(LogLine log) { m_logLine = std::move(log); }

	private:
		void record(const std::string& event, const std::string& owner, const nlohmann::json& detail);
		void publish();

		std::map<std::string, Claim> m_claims;
		std::map<std::string, uint64_t> m_order;   // when each owner first claimed (ties go to the earlier)
		uint64_t m_nextOrder = 0;
		std::string m_tracked;
		Presentation m_presentation;
		uint64_t m_step = 0;
		bool m_changed = true;   // something to publish (a claim, a release, the tracked owner, the presentation)
		LogLine m_logLine;
		std::shared_ptr<Published> m_shared = std::make_shared<Published>();
	};

	// The state and log of a published block, as the tools give them
	nlohmann::json state_of(const Published& shared);
	nlohmann::json log_of(const Published& shared, uint64_t since);
}
