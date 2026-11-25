///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: QuestSystem.h
// Purpose: The quests as they run (docs/QUESTS.md, sections 5, 6, 9, 11): each a state machine over its record's
//          stages, inside the simulation's fixed step. A quest starts (its aliases filled from the binding, or it
//          does not start), claims through the arbiter what its stage shows, leaves a stage on a signal (each
//          subject counted once) or a condition, ends (complete, fail, abandon, its limit) and is torn down (its
//          claim released). Every change goes through one transition function and is logged ({seq, step,
//          serial, record, from, to, outcome, cause}); rewards are keyed by the quest's serial and offered again
//          at every load (the game grants a serial once). The whole state saves to JSON and loads back the same.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "quests/QuestArbiter.h"
#include "quests/QuestBinding.h"
#include "quests/QuestRecord.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <deque>
#include <set>
#include <string>
#include <vector>

namespace Pakal::Quests
{
	struct Quest
	{
		uint64_t serial = 0;
		std::string record;
		int version = 1;
		std::string state = "active";              // active, completed, failed, abandoned; "broken": loaded with its record
		                                           // or stage gone (kept as saved, never stepped, saved back unchanged)
		nlohmann::json kept;                       // a broken quest's saved entry, as it was
		int stage = 0;
		std::map<std::string, Ref> aliases;
		std::map<std::string, double> vars;
		std::map<std::string, std::set<std::string>> counted;   // per exit ("stage:index"): the subjects counted
		// what its stages spawned and still stands, with its source (not saved: the vars "spawned.<source>" and
		// "beaten.<source>" are, and a load makes again only what is still owed)
		std::map<Ref, std::string> spawned;
		bool whileArmed = true;                    // a story quest failed by its while waits for it to hold again (not saved)
		int checkpoint = 0;                        // the last checkpoint stage it entered (0: none; its start)
		uint64_t owedAt = 0;                       // spawns owed (streamed out) looked at again from this step
		uint64_t refillAt = 0;                     // an empty alias is looked for again from this step (not every step)
		double elapsed = 0.0;                      // simulated seconds in its current stage
		double total = 0.0;                        // simulated seconds since it started (its limit)
		std::string owner() const { return record + "#" + std::to_string(serial); }
	};

	class QuestSystem
	{
	public:
		// a quest's reward, offered with its serial: the game grants a serial at most once (MIS-T5b)
		using RewardOffer = std::function<void(uint64_t serial, const std::string& reward)>;
		using LogLine = std::function<void(const std::string& line)>;
		// a level quest's end, told to the game (it keeps its own meta-progress: stars, records): the quest, its outcome
		// ("completed", "failed", "abandoned", or "unfinished" when the level ended first)
		using Outcome = std::function<void(const Quest& quest, const std::string& outcome)>;

		QuestSystem(Arbiter& arbiter, Binding& binding, uint32_t seed) : m_arbiter(arbiter), m_binding(binding), m_seed(seed) {}

		void set_settings(const QuestSettings& settings) { m_settings = settings; }
		const QuestSettings& settings() const { return m_settings; }
		bool add_record(const QuestRecord& record);
		const QuestRecord* record(const std::string& id) const;
		const std::map<std::string, QuestRecord>& records() const { return m_records; }
		void set_reward_offer(RewardOffer offer) { m_rewardOffer = std::move(offer); }
		void set_log_line(LogLine log) { m_logLine = std::move(log); }
		void set_outcome(Outcome outcome) { m_outcome = std::move(outcome); }
		// The level ends: its level quests discarded (their spawns taken away at once, their claims released), each told
		// to the game as "unfinished"; never in a save
		void end_level();
		// What a quest guides to now, for a bot on the simulation's thread (from the core's store, never the presenter's
		// copy): its stage's marker target, and its past or hold line with its way; serial 0: the quest shown
		nlohmann::json guide(uint64_t serial) const;

		// Starts a record now (its aliases filled, or nothing started: 0); 'cause' goes in the log. 'given': aliases a
		// game's director chose itself (a generator that picks the place), each still refused when another quest holds it
		uint64_t start(const std::string& recordId, const std::string& cause, const std::map<std::string, Ref>& given = {});
		// why the last start gave 0 ("" when it started)
		const std::string& last_refusal() const { return m_lastRefusal; }
		// A generator's request: one record of that kind, by weight, among those that may start now (not running unless
		// repeatable, not done unless repeatable, the kind's limit); 0 none
		uint64_t request(const std::string& kind, const std::string& cause);
		// A signal from the game, delivered at the next step(); its subject identifies what it is about
		void signal(const std::string& name, const Ref& subject);

		// A use the player may make now (an exit's 'use'): the player within its meters of the alias's target, the exit's
		// items carried and its other conditions holding. The game offers it its own way (a button, a touch) and calls
		// use() when the player makes it
		struct Offer
		{
			uint64_t serial = 0;
			int stage = 0, exit = 0;
			std::string alias, action;
			Ref target;
			double meters = 0.0, distance = 0.0;
		};
		std::vector<Offer> offers() const;
		// The player made a use: credited at the next step if it is still offered then (same quest, stage and exit, the
		// player still near), else refused (logged)
		void use(uint64_t serial, int stage, int exit);
		// The player's quest items (name -> count), given and handed over by exits; saved with the quests
		const std::map<std::string, int>& items() const { return m_items; }
		bool carries(const std::vector<std::string>& items) const;
		// A tester's (quests.set_item): an item's count set (0: none)
		void set_item(const std::string& name, int count);

		// A choice the player is asked now (a stage's 'choose' exits whose other conditions hold): its prompt, who asks it,
		// and its options in the record's order. The game asks it its own way and calls choose() with the option picked
		struct Choice
		{
			struct Option { int exit = 0; std::string id, text; };
			uint64_t serial = 0;
			int stage = 0;
			std::string prompt, speakerAlias;
			Ref speaker;
			std::vector<Option> options;
		};
		std::vector<Choice> choices() const;
		// The player picked an option: taken at the next step if it is still offered then (else refused, logged). The pick
		// is kept ("record:stage" -> option, saved): what the player chose is never asked again, and a game reads it back
		void choose(uint64_t serial, int stage, const std::string& option);
		// what was picked at a record's stage ("" never asked or not answered yet)
		std::string chosen(const std::string& record, int stage) const;
		void set_var(uint64_t serial, const std::string& name, double value);
		// The player's or the game's command, carried out at the next step()
		void abandon(uint64_t serial);
		void fail(uint64_t serial, const std::string& reason);
		// A tester's: the quest moved to a stage of its record now (logged with cause "qa"), at the next step()
		void force_stage(uint64_t serial, int stage);
		// The quest back at its last checkpoint (its start if none) at the next step: its spawns made anew, its counts
		// cleared (the game's "try again"; a tester's quests.restart_checkpoint)
		void restart_checkpoint(uint64_t serial);
		// A tester's (quests.kill_target): downs a running quest's target through the binding, now: an alias's, or with
		// alias "" every spawn of it still standing. By "world" a protected alias is refused (protection is against the
		// world); by "player" it is not. Answers {killed, protected, refused}
		nlohmann::json kill_target(uint64_t serial, const std::string& alias, const std::string& by, int count = -1);   // count: at most so many spawns (-1 all)
		// A tester's (the bot's teleport-to-alias, quests.teleport_to_alias): the player put 'meters' from a running quest's
		// alias's target (alias "": its first spawn standing). Answers {moved}
		nlohmann::json teleport_to_alias(uint64_t serial, const std::string& alias, double meters);
		// A tester's (R-order: the player at a place before its quest starts): the player put 'meters' from a target
		nlohmann::json bring_player(const Ref& target, double meters)
		{
			TesterBinding* tester = m_binding.tester();
			return { { "moved", target.valid() && tester && tester->bring_player(target, meters) } };
		}
		// A tester's (a route through a stage that waits on the world): a fact of a target put or taken. Answers {set}
		nlohmann::json set_fact(const Ref& target, const std::string& name, bool on)
		{
			TesterBinding* tester = m_binding.tester();
			return { { "set", target.valid() && !name.empty() && tester && tester->set_fact(name, target, on) } };
		}
		// The journal: every quest kept (running and ended), its title, state, stage and objectives, newest first
		nlohmann::json journal() const;
		// A tester's: a flag of the game's store set through the binding (quests.set_flag)
		void set_flag(const std::string& name, double value) { m_binding.set_flag(name, value); }

		// How long its steps took (the last kTimesKept): median, p95, p99 and max in milliseconds, the quests running, and
		// the spawns torn down that wait for the player to be far (despawnWaiting).
		// A measure only: no outcome ever depends on it (the same run plays the same on a slow machine)
		nlohmann::json step_times() const;

		// The simulation's step: the queued signals and commands in order, the conditions, the limits; then the claims
		void step(uint64_t step, double seconds);

		const Quest* quest(uint64_t serial) const;
		const std::map<uint64_t, Quest>& quests() const { return m_quests; }
		bool running(const std::string& recordId) const;
		int done_count(const std::string& recordId) const;

		// The state saved (format 1; elapsed seconds, never steps; aliases as the binding's persistent keys) and loaded
		// back; a load offers every rewarded serial again (the game grants the ones it lacks).
		// A load checks everything before it changes anything: false (an unknown format, a block that is not one) leaves
		// the state as it was, and the game must not write over the saved block; true with errors: the quests that were
		// bad are left out (each one a line), the rest loaded. It never throws.
		nlohmann::json save() const;
		bool load(const nlohmann::json& saved, std::vector<std::string>& errors);

		// Goes up with every change to what save() writes (a transition, a count, a var, a spawn made or beaten, a load):
		// a game saves when it moved, so a kill with no transition still reaches the disk
		uint64_t revision() const { return m_revision; }

		// The transitions' log: one JSON line per change
		const std::deque<nlohmann::json>& log() const { return m_log; }
		// The transitions since the last flush, as "QUEST {...}" lines to the log line set: a game calls it after its
		// step, outside the step's timing (writing lines is the log's cost, not the quests')
		void flush_log();

	private:
		struct Pending { int kind = 0; std::string name; Ref subject; uint64_t serial = 0; std::string reason; int stage = 0; int exit = 0; };
		std::vector<double> m_stepMs;   // the last steps' times (a ring)
		size_t m_stepNext = 0;
		static constexpr size_t kTimesKept = 1200;
		static constexpr uint64_t kRefillSteps = 30;   // a waiting quest looks for a target this often (half a second at 60 Hz)
		void transition(Quest& quest, int to, const std::string& outcome, const std::string& cause);
		// an exit taken: its items handed over and given, then the transition
		void take_exit(Quest& quest, const Exit& exit, const std::string& cause);
		// a use as it would be offered now: its quest, stage and exit, its target and the player's distance (false: not)
		bool offered(const Quest& quest, const Stage& stage, size_t index, Offer* out) const;
		void enter(Quest& quest, int stage, const std::string& cause);
		void end(Quest& quest, const std::string& state, const std::string& cause);
		void claim(const Quest& quest);
		// 'self': a quest whose own holds do not count (a refill of its lost aliases)
		bool fill_aliases(const QuestRecord& record, std::map<std::string, Ref>& out, uint64_t self = 0);
		// a quest's aliases whose target is gone filled again from their rules (all or none); false: still missing
		bool refill_lost(Quest& quest);
		// the aliases marked 'repick' looked at again (every kRefillSteps): true when one moved
		bool repick(Quest& quest, const QuestRecord& record);
		bool load_checked(const nlohmann::json& saved, std::vector<std::string>& errors);
		// a stage's spawns on entry ('owed': after a load, only what was spawned and not yet beaten)
		void make_spawns(Quest& quest, const Stage& stage, bool owed);
		const SpawnRule* spawn_rule(const Quest& quest, const std::string& source) const;
		void beat(Quest& quest, const Ref& ref);
		bool cleared(const Quest& quest, const Stage& stage) const;
		bool holds(const Quest& quest, const Exit& condition) const;   // a 'when' or a 'fact' (a 'while', an exit's)
		double value_of(const Quest& quest, const std::string& var) const;
		void fire_events(Quest& quest, const Stage& stage);
		void protect(const Quest& quest, bool on);
		void despawn_all(Quest& quest, bool now = false);   // now: at once, even near the player (a load replacing the state)
		int priority_of(const QuestRecord& record) const;
		bool may_start(const QuestRecord& record) const;
		void apply_signal(const std::string& name, const Ref& subject);
		void check_conditions(Quest& quest);

		Arbiter& m_arbiter;
		Binding& m_binding;
		// Counter-based draws (splitmix64 of the seed and the count): no std:: distribution, so the same picks on every
		// compiler; the count is saved, so a load between two picks picks as the run that never saved (MIS-T3)
		uint64_t m_seed = 0;
		uint64_t m_draws = 0;
		uint64_t draw();
		double draw01() { return static_cast<double>(draw() >> 11) * (1.0 / 9007199254740992.0); }
		QuestSettings m_settings;
		std::map<std::string, QuestRecord> m_records;
		std::map<uint64_t, Quest> m_quests;            // running and ended (ended ones kept for the count and rewards)
		std::map<std::string, int> m_done;             // record -> times completed
		std::vector<Pending> m_pending;
		std::vector<Ref> m_despawnLater;   // torn down near the player: taken away once it is far (not saved)
		uint64_t m_nextSerial = 1;
		uint64_t m_step = 0;
		uint64_t m_nextSeq = 1;
		uint64_t m_revision = 0;
		std::string m_lastRefusal;
		std::deque<nlohmann::json> m_log;              // the last transitions (kLogKept)
		size_t m_unflushed = 0;                        // the last ones not yet written as lines
		// The last rewards earned (kRewardsKept), saved and offered at load. The bound is safe: a game writes its ledger and
		// the quests' save together, so at most the rewards of one step can be missing from its ledger at a load, never 64
		std::vector<std::pair<uint64_t, std::string>> m_rewarded;
		static constexpr size_t kLogKept = 4000, kRewardsKept = 64;
		void log_entry(const nlohmann::json& entry);
		RewardOffer m_rewardOffer;
		Outcome m_outcome;
		bool level_scoped(const Quest& quest) const;
		void count_holds(Quest& quest, const Stage& stage, double seconds);
		// 'offering': whether a use would be offered, so the use itself is not asked yet
		bool exit_holds(const Quest& quest, const Stage& stage, const Exit& exit, size_t index, bool offering = false) const;
		std::map<std::string, int> m_items;            // the player's quest items (name -> count, none kept at 0)
		std::map<std::string, std::string> m_chosen;   // the choices answered: "record:stage" -> the option (saved)
		LogLine m_logLine;
	};
}
