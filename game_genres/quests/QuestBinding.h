///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: QuestBinding.h
// Purpose: What a genre tells the quests about its world (docs/QUESTS.md, section 7): the candidates for an
//          alias, whether a target still stands, how far the player is from it, the key a target is saved as
//          and found again by, and the marker that points at it. The core never knows what a target is.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "quests/QuestArbiter.h"
#include "quests/QuestRecord.h"

#include <string>
#include <vector>

namespace Pakal::Quests
{
	// A target an alias holds: opaque to the core (the binding's kind and id)
	struct Ref
	{
		std::string kind;
		uint64_t id = 0;
		bool valid() const { return id != 0; }
		bool operator==(const Ref& other) const { return kind == other.kind && id == other.id; }
		bool operator<(const Ref& other) const { return kind != other.kind ? kind < other.kind : id < other.id; }
	};

	// A tester's part of a binding, apart from what the game plays with (the QA tools and the bot's routes): put the
	// player beside a target, down a target as a hit would. A genre that gives it answers Binding::tester()
	class TesterBinding
	{
	public:
		virtual ~TesterBinding() = default;
		// the player put about 'meters' from the target (the bot's teleport); false: cannot
		virtual bool bring_player(const Ref& ref, double meters) = 0;
		// the target downed (killed, destroyed) as a hit would; false: cannot
		virtual bool kill(const Ref& ref) = 0;
		// a world fact of a target put or taken as the world would (a route plays a stage that waits on the world: an
		// outpost cleared); the world may say otherwise later. false: cannot
		virtual bool set_fact(const std::string& name, const Ref& ref, bool on) { (void)name; (void)ref; (void)on; return false; }
	};

	class Binding
	{
	public:
		virtual ~Binding() = default;
		// the candidates for an alias rule, in a stable order (the core picks among them with its own random)
		virtual void find(const AliasRule& rule, std::vector<Ref>& out) const = 0;
		// false: the target is gone (an alias lost; a spawn no longer in the world)
		virtual bool alive(const Ref& ref) const = 0;
		// a target no longer alive because it was killed or destroyed, not merely streamed or culled out: only that
		// counts a spawn beaten (one gone otherwise is owed, made again when the player comes near). The default
		// takes every gone one as defeated: a binding whose world streams things out must say which
		virtual bool defeated(const Ref& ref) const { return !alive(ref); }
		// meters from the player (any player in co-op: the nearest) to it; < 0 unknown
		virtual double distance(const Ref& ref) const = 0;
		// a key that survives a restart (a level object's id, a place's name); "" none: not saved
		virtual std::string persist(const Ref& ref) const = 0;
		virtual Ref restore(const std::string& key) const = 0;
		// the marker that points at it
		virtual Target target(const Ref& ref) const { return Target{ ref.kind, ref.id }; }
		// a stage's spawn: one of 'rule' at 'at' (its ref, or none when it could not be made); owned by the quest
		virtual Ref spawn(const SpawnRule& rule, const Ref& at) { (void)rule; (void)at; return {}; }
		// what the quest spawned and still stands, taken away at its teardown: never in front of the player (out of
		// sight, or later when it is)
		virtual void despawn(const Ref& ref) { (void)ref; }
		// what a tester may do through it (the QA tools, the bot): none unless the genre gives it
		virtual class TesterBinding* tester() { return nullptr; }
		// a target the world must not kill or take while a quest holds it (MIS-V6)
		virtual void protect(const Ref& ref, bool on) { (void)ref; (void)on; }
		// a player past a line: 'axis' ("x", "y") at 'value', going 'way' (+1 the line's growing side, -1 the other), as
		// the genre decides (any player in co-op; a lane does not matter)
		virtual bool passed(const std::string& axis, double value, int way) const { (void)axis; (void)value; (void)way; return false; }
		// a quest went back to a checkpoint stage (restart_checkpoint, onFail "checkpoint"): a genre with its own waves
		// resumes them there
		virtual void restarted(uint64_t serial, int stage) { (void)serial; (void)stage; }
		// the game's flags (its own store, one copy, saved in the same write as the quests: docs/QUESTS.md section 9):
		// a value (0 when unset), and a tester's set (quests.set_flag)
		virtual double flag(const std::string& name) const { (void)name; return 0.0; }
		virtual void set_flag(const std::string& name, double value) { (void)name; (void)value; }
		// a world fact about a target ("opened", "destroyed", "reached"...): unknown facts are false
		virtual bool fact(const std::string& name, const Ref& ref) const { (void)name; (void)ref; return false; }
	};
}
