///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: QuestRecord.h
// Purpose: A quest as data (docs/QUESTS.md, section 4): its stages, objectives, aliases, exits and endings,
//          read from a game's JSON and checked (section 12: MIS-V1 schema, V2 references, V3 completable from
//          every state, V4 the catalog of signals and flags). A record is never changed once read; a running
//          quest is a QuestSystem's.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <nlohmann/json.hpp>

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace Pakal::Quests
{
	// What a stage waits for to leave: a signal (one, or 'count' of them, each subject once), or a condition on the
	// quest's vars; 'to' the stage it goes to
	struct Exit
	{
		std::string signal;        // a name of the catalog ("" when it is a 'when')
		std::string alias;         // the signal's subject must be this alias ("" any): the 'on' condition's own
		// each condition keeps its own target, so one exit may wait on a signal of one alias and a fact of another
		std::string factAlias, reachAlias, holdAlias;
		int count = 1;             // signals needed (each subject counted once)
		std::string whenVar;       // a 'when': the quest's var ("stage.elapsed": the stage's seconds) ...
		std::string whenFlag;      // ... or a flag of the game's store (the binding's)
		double atLeast = 0.0;      // ... at least this
		bool cleared = false;      // every enemy the stage spawned is beaten
		double reachMeters = -1.0; // a 'reach': a player within these meters of 'reachAlias' (the binding's distance)
		// a 'past': a player past a line ('axis' at 'value', going 'way': +1 or -1), as the binding decides (a street's
		// legs go back and forth: the way is the exit's)
		std::string pastAxis;
		double pastValue = 0.0;
		int pastWay = 1;
		// a 'hold': 'holdSeconds' at the place (the exit's past, or within 'holdMeters' of 'holdAlias'), counted in steps;
		// away: "pause" (the count waits), "reset", "fail"; 'holdGrace' seconds away allowed before that
		double holdSeconds = 0.0;
		double holdMeters = -1.0;
		std::string holdAway = "pause";
		double holdGrace = 0.0;
		std::string fact;          // a world fact of 'factAlias' the binding answers ("opened", "destroyed"...): checked on
		                           // entry and every step, so what the player did before is credited (MIS-V7)
		// a 'use': the player uses 'useAlias's target within 'useMeters' (talks to someone, picks something up, installs
		// it). The core never knows how: it offers the use (QuestSystem::offers) while the player is near and the exit's
		// other conditions hold, and the game shows it its own way (a button, a touch) and says when it was made
		// (QuestSystem::use). 'useAction' is the game's word for it ("talk", "pickup", "install"). Only a use made while
		// the stage runs counts: walking by before credits nothing
		std::string useAlias;
		double useMeters = -1.0;
		std::string useAction;
		// the player's quest items (QuestSystem::items, saved with the quests): an exit that 'needs' some is neither
		// offered nor taken without them, and hands them over when it is taken; one that 'gives' some puts them in
		std::vector<std::string> needs, gives;
		int to = 0;                // the stage it goes to
	};

	// What a stage makes when it is entered: 'count' of 'type' at an alias, under a source name (the quest's vars keep,
	// per source, what was spawned and what was beaten: after a load only what is still owed comes back)
	struct SpawnRule
	{
		std::string source;        // a name of its own in the quest ("guards", "wave1")
		std::string kind = "enemy";
		std::string type;          // the binding's (an enemy type)
		std::string at;            // an alias
		int countMin = 1, countMax = 1;
		double near = 0.0;         // made when the player is this near its alias (0: the settings' spawnMeters)
		std::string teardown = "remove";   // when the quest ends otherwise: "remove" (the binding takes it away, out of the
		                                   // player's sight) or "release" (left to the world, no longer the quest's)
	};

	// A stage's timed event: fired once when the stage's elapsed seconds reach 'at' (again after a retry or a restart
	// into the stage): a signal (its subject none), a spawn (the quest's, owed after a load like the stage's own)
	struct TimedEvent
	{
		double at = 0.0;
		std::string signal;
		std::vector<SpawnRule> spawns;
	};

	struct Stage
	{
		int id = 0;
		std::vector<std::string> objectives;   // text: keys or source text (the game's text mode)
		std::string marker;                    // an alias to mark ("" none)
		std::vector<Exit> exits;               // the first that holds wins
		std::string end;                       // "complete", "fail", "abandon": a final stage ("" not final)
		std::string reward;                    // a name the game resolves (a final stage's)
		bool checkpoint = false;
		std::vector<SpawnRule> spawns;         // made on entry, owned by the quest, gone at its teardown
		std::vector<TimedEvent> events;        // by the stage's elapsed seconds
		std::map<std::string, double> set;     // flags put (through the binding) when the stage is entered
		bool hasWhile = false;
		Exit whileCond;                        // must keep holding while the stage is current ('when' or 'fact')
	};

	struct AliasRule
	{
		std::string find;          // a kind the binding knows ("place", "actor", "object", "point")
		std::string type;          // the binding's sub-kind ("cache", an enemy type...)
		std::string pick = "nearest";   // "nearest", "random", "named"
		std::string name;          // with "named"
		double minMeters = 0.0, maxMeters = 1e9;
		bool reserve = true;
		bool protect = false;      // kept alive and untaken by the world while the quest holds it (MIS-V6)
		std::string lost = "fail"; // when its target is gone: "fail", or "refill" (another from its rule)
		bool optional = false;     // the quest starts without it when the world has no candidate (a guiding mark: the
		                           // place it would point at may be done already); 'repick' is optional too
		bool repick = false;       // looked at again now and then: another candidate taken when its own is no longer one, or
		                           // another is clearly nearer (a guiding mark: the nearest open place of a kind)
		nlohmann::json where = nlohmann::json::object();   // the binding's own filters (opened: false...)
	};

	struct QuestRecord
	{
		int format = 1;
		std::string id;
		int version = 1;
		std::string title, brief;
		std::string kind;          // the game's category (errand, story...)
		int priority = 50;         // the arbiter's (a game's kind default when left out: QuestSystem)
		bool repeatable = false;
		bool concurrent = false;   // (repeatable) more than one at once, each on its own targets (an outpost's capture, one
		                           // per outpost: the reservations keep two off one target)
		std::string startsBy = "generator";   // story, generator, signal, manual
		std::string then;          // a record started when this one completes (a story's chain)
		std::string scope = "world";   // "world" (saved, outlives levels) or "level" (started with a level, discarded with
		                               // it, never saved; its outcome told to the game)
		std::string onFail;        // "retry" (from its start), "checkpoint" (from its last checkpoint), a record id, or ""
		double weight = 1.0;       // among a kind's records, for a generator
		double limitSeconds = 0.0; // 0: none
		std::map<std::string, AliasRule> aliases;
		std::vector<Stage> stages; // the first is where it starts
		std::vector<Exit> fail;    // from any stage: to the record's fail (Exit::to unused)
		bool hasWhile = false;
		Exit whileCond;            // must keep holding while the quest runs; when it stops, the quest fails (a story
		                           // quest through its onFail, once: it waits for the condition before failing again)
		nlohmann::json game = nlohmann::json::object();   // the record's opaque block for the game

		const Stage* stage(int id) const;
	};

	// The game's quest settings: its text mode, its kinds' priorities, the catalog (every signal with its emitter, every
	// flag with its one owner), its limits per kind
	struct QuestSettings
	{
		std::string textMode = "keys";
		std::map<std::string, int> kindPriority;
		std::map<std::string, std::string> signals;   // name -> emitter ("binding", "code", "fragment:<name>")
		std::map<std::string, std::string> flags;     // name -> owner
		std::map<std::string, int> limits;            // kind -> quests of it at once
		double despawnMeters = 0.0;                   // a spawn torn down is taken away only this far from the player (0: at once)
		double spawnMeters = 0.0;                     // a spawn owed is made when the player is this near its alias (0: always)
		std::set<std::string> beaten = { "enemy_down" };   // the signals that count a quest's spawn beaten (their subject)
		std::map<std::string, std::string> items;     // the quest items: name -> its text (what the game shows it as)
	};

	// Reads a record (false: 'errors' says why, one line each: MIS-V1)
	bool read_record(const nlohmann::json& json, QuestRecord& out, std::vector<std::string>& errors);
	bool read_settings(const nlohmann::json& json, QuestSettings& out, std::vector<std::string>& errors);

	// What a check knows of the game: is a text translated (MIS-V2), is an alias kind one the binding resolves
	struct CheckContext
	{
		std::function<bool(const std::string& text)> textKnown;    // null: texts not checked
		std::set<std::string> aliasKinds = { "place", "actor", "object", "point" };
		std::set<std::string> rewards;                             // empty: rewards not checked
	};

	// MIS-V2..V4 on a record against the settings: its exits' signals in the catalog, its stages reachable and each
	// with a way to an end (a dead end only when marked final), its aliases' kinds known, its texts translated, a
	// story record's onFail. One line per finding, empty when it is sound
	std::vector<std::string> check_record(const QuestRecord& record, const QuestSettings& settings, const CheckContext& context);
	// Checks across the records: every 'then' and every named 'onFail' is a record, and no chain of them comes back to
	// where it started (a cycle a chain credited at once would run forever); every item an exit needs is given by some
	// exit (or the player could never have it). One line per finding
	std::vector<std::string> check_chains(const std::map<std::string, QuestRecord>& records);
	// What an exit waits on, in a line ("signal cache_opened of cache", "past x 140 going +1 and flag waves >= 1"): the
	// tools' (quests.state, the quests' panel) reading of a stuck stage
	std::string describe_exit(const Exit& exit);
}
