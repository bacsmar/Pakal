#include "quests/QuestSystem.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <climits>
#include <cmath>
#include <cstdint>

namespace Pakal::Quests
{
	namespace
	{
		std::string subject_key(const Ref& ref) { return ref.kind + ":" + std::to_string(ref.id); }
		enum { kSignal = 1, kAbandon = 2, kFail = 3, kForce = 4, kRestart = 5, kStart = 6, kUse = 7 };
	}

	bool QuestSystem::add_record(const QuestRecord& record)
	{
		if (record.id.empty()) return false;
		m_records[record.id] = record;
		return true;
	}

	const QuestRecord* QuestSystem::record(const std::string& id) const
	{
		auto found = m_records.find(id);
		return found != m_records.end() ? &found->second : nullptr;
	}

	const Quest* QuestSystem::quest(uint64_t serial) const
	{
		auto found = m_quests.find(serial);
		return found != m_quests.end() ? &found->second : nullptr;
	}

	bool QuestSystem::running(const std::string& recordId) const
	{
		for (const auto& [serial, q] : m_quests) if (q.record == recordId && q.state == "active") return true;
		return false;
	}

	int QuestSystem::done_count(const std::string& recordId) const
	{
		auto found = m_done.find(recordId);
		return found != m_done.end() ? found->second : 0;
	}

	int QuestSystem::priority_of(const QuestRecord& record) const
	{
		if (record.priority >= 0) return record.priority;
		auto found = m_settings.kindPriority.find(record.kind);
		return found != m_settings.kindPriority.end() ? found->second : 50;
	}

	bool QuestSystem::may_start(const QuestRecord& record) const
	{
		if (!record.repeatable && (running(record.id) || done_count(record.id) > 0)) return false;
		if (record.repeatable && !record.concurrent && running(record.id)) return false;
		auto limit = m_settings.limits.find(record.kind);
		if (limit != m_settings.limits.end())
		{
			int live = 0;
			for (const auto& [serial, q] : m_quests)
				if (q.state == "active") { const QuestRecord* r = this->record(q.record); live += r && r->kind == record.kind ? 1 : 0; }
			if (live >= limit->second) return false;
		}
		return true;
	}

	bool QuestSystem::fill_aliases(const QuestRecord& record, std::map<std::string, Ref>& out, uint64_t self)
	{
		// (out may hold aliases already given: they are kept, if no running quest holds them)
		// what the running quests hold and reserve: never given to another
		std::set<Ref> held;
		for (const auto& [serial, q] : m_quests)
		{
			if (q.state != "active" || q.serial == self) continue;
			const QuestRecord* r = this->record(q.record);
			for (const auto& [name, ref] : q.aliases)
				if (r && r->aliases.count(name) && r->aliases.at(name).reserve) held.insert(ref);
		}
		for (const auto& [name, rule] : record.aliases)
		{
			auto given = out.find(name);
			if (given != out.end())
			{
				if (!given->second.valid() || (rule.reserve && held.count(given->second))) return false;
				if (rule.reserve) held.insert(given->second);
				continue;
			}
			std::vector<Ref> candidates;
			m_binding.find(rule, candidates);
			std::vector<std::pair<double, Ref>> fit;
			for (const Ref& ref : candidates)
			{
				if (!ref.valid() || held.count(ref)) continue;
				const double d = m_binding.distance(ref);
				if (d >= 0.0 && (d < rule.minMeters || d > rule.maxMeters)) continue;
				fit.push_back({ d < 0.0 ? 1e18 : d, ref });
			}
			if (fit.empty())
			{
				// (a guiding mark with no candidate now starts empty: it is looked at again; what it guides to may be done already)
				if (rule.optional) { out[name] = Ref{}; continue; }
				return false;   // (an alias that cannot be filled: the quest does not start, nothing left behind)
			}
			Ref chosen;
			if (rule.pick == "random") chosen = fit[draw() % fit.size()].second;
			else if (rule.pick == "named") chosen = fit.front().second;
			else chosen = std::min_element(fit.begin(), fit.end(), [](const auto& a, const auto& b) { return a.first < b.first; })->second;
			out[name] = chosen;
			if (rule.reserve) held.insert(chosen);
		}
		return true;
	}

	uint64_t QuestSystem::start(const std::string& recordId, const std::string& cause, const std::map<std::string, Ref>& given)
	{
		const QuestRecord* record = this->record(recordId);
		// (why a start gives 0, for the game to log)
		if (!record) { m_lastRefusal = "no record '" + recordId + "'"; return 0; }
		if (record->stages.empty()) { m_lastRefusal = "the record has no stage"; return 0; }
		if (!may_start(*record))
		{
			m_lastRefusal = running(record->id) ? "it runs already" : (!record->repeatable && done_count(record->id) > 0) ? "done already (not repeatable)"
				: "its kind's limit (" + record->kind + ") is reached";
			return 0;
		}
		std::map<std::string, Ref> aliases = given;
		if (!fill_aliases(*record, aliases)) { m_lastRefusal = "an alias could not be filled (given one held or invalid, or no candidate)"; return 0; }
		m_lastRefusal.clear();
		Quest quest;
		quest.serial = m_nextSerial++;
		quest.record = record->id;
		quest.version = record->version;
		quest.aliases = aliases;
		quest.stage = 0;
		Quest& live = m_quests[quest.serial] = quest;
		protect(live, true);
		transition(live, record->stages.front().id, "start", cause);
		return live.serial;
	}

	uint64_t QuestSystem::request(const std::string& kind, const std::string& cause)
	{
		std::vector<const QuestRecord*> pool;
		double total = 0.0;
		for (const auto& [id, record] : m_records)
			if (record.kind == kind && record.startsBy == "generator" && may_start(record)) { pool.push_back(&record); total += std::max(0.0, record.weight); }
		// by weight, then the next ones if the chosen one cannot fill its aliases
		while (!pool.empty() && total > 0.0)
		{
			double pick = draw01() * total;
			size_t chosen = pool.size() - 1;
			for (size_t i = 0; i < pool.size(); ++i) { pick -= std::max(0.0, pool[i]->weight); if (pick <= 0.0) { chosen = i; break; } }
			if (const uint64_t serial = start(pool[chosen]->id, cause)) return serial;
			total -= std::max(0.0, pool[chosen]->weight);
			pool.erase(pool.begin() + static_cast<long>(chosen));
		}
		return 0;
	}

	void QuestSystem::log_entry(const nlohmann::json& entry)
	{
		++m_revision;
		m_log.push_back(entry);
		while (m_log.size() > kLogKept) m_log.pop_front();
		++m_unflushed;
	}

	void QuestSystem::flush_log()
	{
		const size_t n = std::min(m_unflushed, m_log.size());
		m_unflushed = 0;
		if (!m_logLine) return;
		for (size_t i = m_log.size() - n; i < m_log.size(); ++i) m_logLine("QUEST " + m_log[i].dump());
	}

	uint64_t QuestSystem::draw()
	{
		uint64_t z = (m_seed ^ (m_seed << 32)) + (++m_draws) * 0x9E3779B97F4A7C15ull;
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
		return z ^ (z >> 31);
	}

	void QuestSystem::signal(const std::string& name, const Ref& subject) { m_pending.push_back({ kSignal, name, subject, 0, {} }); }
	void QuestSystem::abandon(uint64_t serial) { m_pending.push_back({ kAbandon, {}, {}, serial, {} }); }
	void QuestSystem::fail(uint64_t serial, const std::string& reason) { m_pending.push_back({ kFail, {}, {}, serial, reason }); }

	void QuestSystem::force_stage(uint64_t serial, int stage) { m_pending.push_back({ kForce, {}, {}, serial, {}, stage }); }
	void QuestSystem::use(uint64_t serial, int stage, int exit) { m_pending.push_back({ kUse, {}, {}, serial, {}, stage, exit }); }

	bool QuestSystem::carries(const std::vector<std::string>& items) const
	{
		std::map<std::string, int> wanted;
		for (const std::string& item : items) ++wanted[item];
		for (const auto& [item, n] : wanted)
		{
			auto held = m_items.find(item);
			if (held == m_items.end() || held->second < n) return false;
		}
		return true;
	}

	void QuestSystem::set_item(const std::string& name, int count)
	{
		if (name.empty()) return;
		if (count > 0) m_items[name] = count;
		else m_items.erase(name);
		++m_revision;
	}

	bool QuestSystem::offered(const Quest& quest, const Stage& stage, size_t index, Offer* out) const
	{
		const Exit& exit = stage.exits[index];
		if (exit.useMeters < 0.0 || !exit.signal.empty()) return false;   // (a use with a signal is not offered: the signal decides)
		auto held = quest.aliases.find(exit.useAlias);
		if (held == quest.aliases.end() || !held->second.valid()) return false;
		const double d = m_binding.distance(held->second);
		if (d < 0.0 || d > exit.useMeters) return false;
		if (!exit_holds(quest, stage, exit, index, true)) return false;
		if (out) *out = Offer{ quest.serial, stage.id, static_cast<int>(index), exit.useAlias, exit.useAction, held->second, exit.useMeters, d };
		return true;
	}

	std::vector<QuestSystem::Offer> QuestSystem::offers() const
	{
		std::vector<Offer> out;
		for (const auto& [serial, quest] : m_quests)
		{
			if (quest.state != "active") continue;
			const QuestRecord* record = this->record(quest.record);
			const Stage* stage = record ? record->stage(quest.stage) : nullptr;
			if (!stage) continue;
			for (size_t i = 0; i < stage->exits.size(); ++i)
			{
				Offer offer;
				if (offered(quest, *stage, i, &offer)) out.push_back(offer);
			}
		}
		std::sort(out.begin(), out.end(), [](const Offer& a, const Offer& b) { return a.distance < b.distance; });   // (the nearest first)
		return out;
	}

	void QuestSystem::take_exit(Quest& quest, const Exit& exit, const std::string& cause)
	{
		if (!exit.needs.empty() || !exit.gives.empty())
		{
			for (const std::string& item : exit.needs)
				if (--m_items[item] <= 0) m_items.erase(item);
			for (const std::string& item : exit.gives) ++m_items[item];
			nlohmann::json items = nlohmann::json::object();
			for (const auto& [name, n] : m_items) items[name] = n;
			log_entry({ { "seq", m_nextSeq++ }, { "step", m_step }, { "serial", quest.serial }, { "record", quest.record }, { "from", quest.stage },
				{ "to", quest.stage }, { "outcome", "items" }, { "cause", cause }, { "handed", exit.needs }, { "given", exit.gives }, { "items", items } });
			++m_revision;
		}
		transition(quest, exit.to, "next", cause);
	}
	void QuestSystem::restart_checkpoint(uint64_t serial) { m_pending.push_back({ kRestart, {}, {}, serial, {} }); }

	nlohmann::json QuestSystem::teleport_to_alias(uint64_t serial, const std::string& alias, double meters)
	{
		auto found = m_quests.find(serial);
		if (found == m_quests.end() || found->second.state != "active") return { { "error", "no running quest " + std::to_string(serial) } };
		const Quest& quest = found->second;
		Ref target;
		if (alias.empty()) { for (const auto& [ref, source] : quest.spawned) if (m_binding.alive(ref)) { target = ref; break; } }
		else if (auto held = quest.aliases.find(alias); held != quest.aliases.end()) target = held->second;
		if (!target.valid()) return { { "error", "no target for the alias '" + alias + "'" } };
		TesterBinding* tester = m_binding.tester();
		return { { "moved", tester && tester->bring_player(target, meters) } };
	}

	nlohmann::json QuestSystem::kill_target(uint64_t serial, const std::string& alias, const std::string& by, int count)
	{
		auto found = m_quests.find(serial);
		if (found == m_quests.end() || found->second.state != "active") return { { "error", "no running quest " + std::to_string(serial) } };
		Quest& quest = found->second;
		int killed = 0;
		if (alias.empty())
		{
			std::vector<Ref> standing;
			for (const auto& [ref, source] : quest.spawned) if (m_binding.alive(ref)) standing.push_back(ref);
			for (const Ref& ref : standing)
			{
				if (count >= 0 && killed >= count) break;
				killed += m_binding.tester() && m_binding.tester()->kill(ref) ? 1 : 0;
			}
			return { { "killed", killed }, { "protected", false }, { "refused", false } };
		}
		const QuestRecord* record = this->record(quest.record);
		auto held = quest.aliases.find(alias);
		if (!record || !record->aliases.count(alias) || held == quest.aliases.end() || !held->second.valid())
			return { { "error", "the quest has no target for the alias '" + alias + "'" } };
		const bool guarded = record->aliases.at(alias).protect;
		if (guarded && by != "player") return { { "killed", 0 }, { "protected", true }, { "refused", true } };
		killed = m_binding.tester() && m_binding.tester()->kill(held->second) ? 1 : 0;
		return { { "killed", killed }, { "protected", guarded }, { "refused", false } };
	}

	nlohmann::json QuestSystem::step_times() const
	{
		std::vector<double> times = m_stepMs;
		int live = 0;
		for (const auto& [serial, q] : m_quests) live += q.state == "active" ? 1 : 0;
		if (times.empty()) return { { "steps", 0 }, { "running", live }, { "despawnWaiting", m_despawnLater.size() } };
		std::sort(times.begin(), times.end());
		auto at = [&](double share) { return times[std::min(times.size() - 1, static_cast<size_t>(share * static_cast<double>(times.size())))]; };
		return { { "steps", times.size() }, { "despawnWaiting", m_despawnLater.size() }, { "medianMs", at(0.5) }, { "p95Ms", at(0.95) }, { "p99Ms", at(0.99) }, { "maxMs", times.back() }, { "running", live } };
	}

	void QuestSystem::set_var(uint64_t serial, const std::string& name, double value)
	{
		auto found = m_quests.find(serial);
		if (found != m_quests.end() && found->second.state == "active") { found->second.vars[name] = value; ++m_revision; }
	}

	void QuestSystem::claim(const Quest& quest)
	{
		const QuestRecord* record = this->record(quest.record);
		const Stage* stage = record ? record->stage(quest.stage) : nullptr;
		if (!stage) return;
		Claim c;
		c.priority = priority_of(*record);
		auto marked = quest.aliases.find(stage->marker);
		if (!stage->marker.empty() && marked != quest.aliases.end() && marked->second.valid()) c.marker = m_binding.target(marked->second);
		for (const std::string& line : stage->objectives) c.objectives.push_back({ line });
		m_arbiter.claim(quest.owner(), c);
	}

	void QuestSystem::transition(Quest& quest, int to, const std::string& outcome, const std::string& cause)
	{
		nlohmann::json entry = { { "seq", m_nextSeq++ }, { "step", m_step }, { "serial", quest.serial }, { "record", quest.record },
			{ "from", quest.stage }, { "to", to }, { "outcome", outcome }, { "cause", cause } };
		log_entry(entry);
		quest.stage = to;
		quest.elapsed = 0.0;
		quest.counted.clear();
		const QuestRecord* record = this->record(quest.record);
		const Stage* stage = record ? record->stage(to) : nullptr;
		if (stage) for (const auto& [name, value] : stage->set) m_binding.set_flag(name, value);
		if (stage && !stage->end.empty())
		{
			end(quest, stage->end == "complete" ? "completed" : stage->end == "fail" ? "failed" : "abandoned", cause);
			return;
		}
		if (stage && stage->checkpoint) quest.checkpoint = to;
		// a stage entered (again: a checkpoint restart, a loop back) starts clean: its timed events fire anew and its holds
		// count from nothing, their away timers too (a hold of 25 s is 25 s after the restart)
		const std::string fired = "event." + std::to_string(to) + ".", held = "hold." + std::to_string(to) + ".", used = "use." + std::to_string(to) + ".";
		for (auto it = quest.vars.begin(); it != quest.vars.end();)
			it = it->first.rfind(fired, 0) == 0 || it->first.rfind(held, 0) == 0 || it->first.rfind(used, 0) == 0 ? quest.vars.erase(it) : std::next(it);
		if (stage) make_spawns(quest, *stage, false);
		claim(quest);
		check_conditions(quest);   // (what already holds is credited on entry: a 'when', a fact true at once; MIS-V7)
	}

	void QuestSystem::end(Quest& quest, const std::string& state, const std::string& cause)
	{
		if (quest.state != "active") return;   // (ends once)
		const QuestRecord* record = this->record(quest.record);
		// a story quest never stops: its fail retries from its start, or hands over to its follow-up; it is never abandoned
		if (record && record->startsBy == "story" && state != "completed")
		{
			if (state == "abandoned")
			{
				nlohmann::json entry = { { "seq", m_nextSeq++ }, { "step", m_step }, { "serial", quest.serial }, { "record", quest.record },
					{ "from", quest.stage }, { "to", quest.stage }, { "outcome", "refused" }, { "cause", "abandon of a story quest: " + cause } };
				log_entry(entry);
				return;
			}
			if (record->onFail == "retry" || record->onFail == "checkpoint")
			{
				despawn_all(quest);   // (the retry makes its own again)
				for (auto& [name, ref] : quest.aliases)
					if (ref.valid() && !m_binding.alive(ref))
					{
						if (record->aliases.count(name) && record->aliases.at(name).protect) m_binding.protect(ref, false);   // (lost: no longer the quest's)
						ref = Ref{};
					}
				refill_lost(quest);
				const int from = record->onFail == "checkpoint" && quest.checkpoint && record->stage(quest.checkpoint) ? quest.checkpoint : record->stages.front().id;
				transition(quest, from, "retry", cause);
				if (record->onFail == "checkpoint") m_binding.restarted(quest.serial, from);
				return;
			}
		}
		quest.state = state;
		m_arbiter.release(quest.owner());
		// the teardown: nothing of the quest stays behind (its spawns gone, its targets free to the world again)
		despawn_all(quest);
		protect(quest, false);
		nlohmann::json entry = { { "seq", m_nextSeq++ }, { "step", m_step }, { "serial", quest.serial }, { "record", quest.record },
			{ "from", quest.stage }, { "to", quest.stage }, { "outcome", state }, { "cause", cause } };
		const Stage* stage = record ? record->stage(quest.stage) : nullptr;
		if (state == "completed")
		{
			++m_done[quest.record];
			if (stage && !stage->reward.empty())
			{
				entry["rewards"] = { stage->reward };
				m_rewarded.push_back({ quest.serial, stage->reward });
				if (m_rewarded.size() > kRewardsKept) m_rewarded.erase(m_rewarded.begin());
				if (m_rewardOffer) m_rewardOffer(quest.serial, stage->reward);
			}
		}
		log_entry(entry);
		if (m_outcome && level_scoped(quest)) m_outcome(quest, state);
		if (record && record->startsBy == "story" && state == "failed" && !record->onFail.empty() && record->onFail != "retry" && record->onFail != "checkpoint")
			m_pending.push_back({ kStart, record->onFail, {}, 0, "follow-up of " + quest.record });   // (at the next step: never inside an end)
		if (record && state == "completed" && !record->then.empty())
			m_pending.push_back({ kStart, record->then, {}, 0, "then of " + quest.record });   // (at the next step: a chain credited at once never recurses)
	}

	void QuestSystem::apply_signal(const std::string& name, const Ref& subject)
	{
		for (auto& [serial, quest] : m_quests)
		{
			if (quest.state != "active") continue;
			const QuestRecord* record = this->record(quest.record);
			const Stage* stage = record ? record->stage(quest.stage) : nullptr;
			if (!stage) continue;
			if (m_settings.beaten.count(name)) beat(quest, subject);
			auto matches = [&](const Exit& exit)
			{
				if (exit.signal != name) return false;
				if (exit.alias.empty()) return true;
				auto held = quest.aliases.find(exit.alias);
				return held != quest.aliases.end() && held->second == subject;
			};
			bool left = false;
			for (const Exit& exit : record->fail)
			{
				if (!matches(exit)) continue;
				end(quest, "failed", "signal:" + name + "#" + subject_key(subject));
				left = true;
				break;
			}
			if (left) continue;
			for (size_t i = 0; i < stage->exits.size(); ++i)
			{
				const Exit& exit = stage->exits[i];
				if (!matches(exit)) continue;
				std::set<std::string>& seen = quest.counted[std::to_string(quest.stage) + ":" + std::to_string(i)];
				if (seen.insert(subject_key(subject)).second) ++m_revision;   // (each subject counted once: the same signal twice changes nothing)
				if (static_cast<int>(seen.size()) >= exit.count && exit_holds(quest, *stage, exit, i))
				{
					take_exit(quest, exit, "signal:" + name + "#" + subject_key(subject));
					break;
				}
			}
		}
	}

	bool QuestSystem::repick(Quest& quest, const QuestRecord& record)
	{
		constexpr double kSticky = 0.8;   // (another is taken only when clearly nearer)
		bool moved = false;
		for (const auto& [name, rule] : record.aliases)
		{
			if (!rule.repick) continue;
			// what other running quests hold is not a candidate
			std::set<Ref> held;
			for (const auto& [serial, q] : m_quests)
			{
				if (q.state != "active" || q.serial == quest.serial) continue;
				const QuestRecord* r = this->record(q.record);
				for (const auto& [n, ref] : q.aliases) if (r && r->aliases.count(n) && r->aliases.at(n).reserve) held.insert(ref);
			}
			std::vector<Ref> candidates;
			m_binding.find(rule, candidates);
			const Ref current = quest.aliases.count(name) ? quest.aliases.at(name) : Ref{};
			Ref best;
			double bestD = 1e18, currentD = -1.0;
			for (const Ref& ref : candidates)
			{
				if (!ref.valid() || (rule.reserve && held.count(ref))) continue;
				const double d = m_binding.distance(ref);
				if (d >= 0.0 && (d < rule.minMeters || d > rule.maxMeters)) continue;
				const double key = d < 0.0 ? 1e17 : d;
				if (ref == current) currentD = key;
				if (key < bestD) { bestD = key; best = ref; }
			}
			Ref chosen = current;
			if (currentD < 0.0) chosen = best;                                      // (its own no longer a candidate)
			else if (best.valid() && !(best == current) && bestD < currentD * kSticky) chosen = best;
			if (chosen == current) continue;
			if (rule.protect && current.valid()) m_binding.protect(current, false);
			if (rule.protect && chosen.valid()) m_binding.protect(chosen, true);
			quest.aliases[name] = chosen;
			moved = true;
			log_entry({ { "seq", m_nextSeq++ }, { "step", m_step }, { "serial", quest.serial }, { "record", quest.record },
				{ "from", quest.stage }, { "to", quest.stage }, { "outcome", "repick" }, { "cause", "alias:" + name } });
		}
		if (moved) claim(quest);
		return moved;
	}

	bool QuestSystem::refill_lost(Quest& quest)
	{
		const QuestRecord* record = this->record(quest.record);
		if (!record) return false;
		std::map<std::string, Ref> kept;
		for (const auto& [name, ref] : quest.aliases) if (ref.valid()) kept[name] = ref;
		if (kept.size() == record->aliases.size()) return true;
		if (!fill_aliases(*record, kept, quest.serial)) return false;
		for (auto& [name, ref] : kept)
			if (ref.valid() && (!quest.aliases.count(name) || !quest.aliases[name].valid()))   // (a mark still empty: no line)
				log_entry({ { "seq", m_nextSeq++ }, { "step", m_step }, { "serial", quest.serial }, { "record", quest.record },
					{ "from", quest.stage }, { "to", quest.stage }, { "outcome", "refill" }, { "cause", "alias:" + name } });
		// the new targets protected as the start's were (MIS-V6): a retry's new giver is kept from the world too
		for (const auto& [name, ref] : kept)
		{
			auto was = quest.aliases.find(name);
			const bool fresh = was == quest.aliases.end() || !(was->second == ref);
			if (fresh && ref.valid() && record->aliases.count(name) && record->aliases.at(name).protect) m_binding.protect(ref, true);
		}
		quest.aliases = kept;
		claim(quest);
		return true;
	}

	void QuestSystem::check_conditions(Quest& quest)
	{
		if (quest.state != "active") return;
		const QuestRecord* record = this->record(quest.record);
		const Stage* stage = record ? record->stage(quest.stage) : nullptr;
		if (!stage) return;
		// a 'while' that stopped holding: the quest fails (a story quest's fail goes through its onFail once, then waits
		// for the condition to hold again before it can fail by it again: never a loop)
		const Exit* stopped = record->hasWhile && !holds(quest, record->whileCond) ? &record->whileCond
			: stage->hasWhile && !holds(quest, stage->whileCond) ? &stage->whileCond : nullptr;
		if (!stopped) quest.whileArmed = true;
		else if (quest.whileArmed)
		{
			quest.whileArmed = false;
			end(quest, "failed", "while");
			return;
		}
		for (size_t i = 0; i < stage->exits.size(); ++i)
		{
			const Exit& exit = stage->exits[i];
			if (!exit.signal.empty()) continue;   // (a signal's exit is taken where the signal arrives)
			if (!exit_holds(quest, *stage, exit, i)) continue;
			std::string cause = exit.useMeters >= 0.0 ? "use:" + exit.useAlias : !exit.fact.empty() ? "fact:" + exit.fact : exit.reachMeters >= 0.0 ? "reach:" + exit.reachAlias
				: exit.holdSeconds > 0.0 ? "hold" : !exit.pastAxis.empty() ? "past:" + exit.pastAxis : exit.cleared ? "cleared"
				: !exit.whenVar.empty() || !exit.whenFlag.empty() ? "when:" + (exit.whenVar.empty() ? "flag." + exit.whenFlag : exit.whenVar) : "items";
			take_exit(quest, exit, cause);
			return;
		}
	}

	double QuestSystem::value_of(const Quest& quest, const std::string& var) const
	{
		if (var == "stage.elapsed") return quest.elapsed;
		auto value = quest.vars.find(var);
		return value != quest.vars.end() ? value->second : 0.0;
	}

	bool QuestSystem::holds(const Quest& quest, const Exit& condition) const
	{
		if (!condition.whenVar.empty()) return value_of(quest, condition.whenVar) >= condition.atLeast && (condition.whenVar == "stage.elapsed" || quest.vars.count(condition.whenVar));
		if (!condition.whenFlag.empty()) return m_binding.flag(condition.whenFlag) >= condition.atLeast;
		if (!condition.fact.empty())
		{
			auto held = quest.aliases.find(condition.factAlias);
			return held != quest.aliases.end() && held->second.valid() && m_binding.fact(condition.fact, held->second);
		}
		if (condition.reachMeters >= 0.0)
		{
			auto held = quest.aliases.find(condition.reachAlias);
			const double d = held != quest.aliases.end() && held->second.valid() ? m_binding.distance(held->second) : -1.0;
			return d >= 0.0 && d <= condition.reachMeters;
		}
		return true;
	}

	// An exit holds when ALL its conditions do (a leg's place reached AND its waves beaten); a signal exit's signal is
	// counted where it arrives, its other conditions checked with it
	bool QuestSystem::exit_holds(const Quest& quest, const Stage& stage, const Exit& exit, size_t index, bool offering) const
	{
		if (!exit.needs.empty() && !carries(exit.needs)) return false;
		if (exit.useMeters >= 0.0 && !offering && !quest.vars.count("use." + std::to_string(stage.id) + "." + std::to_string(index))) return false;
		if ((!exit.whenVar.empty() || !exit.whenFlag.empty()))
		{
			Exit when;
			when.whenVar = exit.whenVar;
			when.whenFlag = exit.whenFlag;
			when.atLeast = exit.atLeast;
			if (!holds(quest, when)) return false;
		}
		if (!exit.fact.empty())
		{
			auto held = quest.aliases.find(exit.factAlias);
			if (held == quest.aliases.end() || !held->second.valid() || !m_binding.fact(exit.fact, held->second)) return false;
		}
		if (exit.reachMeters >= 0.0)
		{
			Exit reach;
			reach.reachAlias = exit.reachAlias;
			reach.reachMeters = exit.reachMeters;
			if (!holds(quest, reach)) return false;
		}
		if (exit.cleared && !cleared(quest, stage)) return false;
		if (exit.holdSeconds > 0.0)
		{
			auto held = quest.vars.find("hold." + std::to_string(stage.id) + "." + std::to_string(index));
			if (held == quest.vars.end() || held->second < exit.holdSeconds) return false;
		}
		else if (!exit.pastAxis.empty() && !m_binding.passed(exit.pastAxis, exit.pastValue, exit.pastWay)) return false;
		return true;
	}

	// A hold's seconds: counted while the player is there (the exit's past, or within meters of its alias); away, after
	// its grace: the count waits ("pause"), starts again ("reset"), or the quest fails ("fail")
	void QuestSystem::count_holds(Quest& quest, const Stage& stage, double seconds)
	{
		for (size_t i = 0; i < stage.exits.size(); ++i)
		{
			const Exit& exit = stage.exits[i];
			if (exit.holdSeconds <= 0.0) continue;
			bool there = false;
			if (!exit.pastAxis.empty()) there = m_binding.passed(exit.pastAxis, exit.pastValue, exit.pastWay);
			else
			{
				auto held = quest.aliases.find(exit.holdAlias);
				const double d = held != quest.aliases.end() && held->second.valid() ? m_binding.distance(held->second) : -1.0;
				there = d >= 0.0 && d <= exit.holdMeters;
			}
			const std::string key = "hold." + std::to_string(stage.id) + "." + std::to_string(i), away = key + ".away";
			if (there) { quest.vars[key] += seconds; quest.vars[away] = 0.0; ++m_revision; continue; }
			if (!quest.vars.count(key)) continue;   // (not begun: nothing to lose)
			quest.vars[away] += seconds;
			if (quest.vars[away] <= exit.holdGrace) continue;
			if (exit.holdAway == "reset" && quest.vars[key] > 0.0) { quest.vars[key] = 0.0; ++m_revision; }
			else if (exit.holdAway == "fail") { end(quest, "failed", "hold: away"); return; }
		}
	}

	void QuestSystem::fire_events(Quest& quest, const Stage& stage)
	{
		for (size_t i = 0; i < stage.events.size(); ++i)
		{
			const TimedEvent& event = stage.events[i];
			const std::string key = "event." + std::to_string(stage.id) + "." + std::to_string(i);
			if (quest.elapsed < event.at || quest.vars.count(key)) continue;
			quest.vars[key] = 1.0;   // (fired: saved with the vars, so a load does not fire it twice)
			log_entry({ { "seq", m_nextSeq++ }, { "step", m_step }, { "serial", quest.serial }, { "record", quest.record },
				{ "from", quest.stage }, { "to", quest.stage }, { "outcome", "event" }, { "cause", "at:" + std::to_string(event.at) } });
			if (!event.signal.empty()) m_pending.push_back({ kSignal, event.signal, Ref{}, 0, {} });
			if (!event.spawns.empty())
			{
				Stage once;
				once.id = stage.id;
				once.spawns = event.spawns;
				make_spawns(quest, once, false);
			}
		}
	}

	void QuestSystem::make_spawns(Quest& quest, const Stage& stage, bool owed)
	{
		// (owed: the stage's own and its events' already fired, made again where they are not standing)
		std::vector<SpawnRule> rules = stage.spawns;
		if (owed)
			for (size_t i = 0; i < stage.events.size(); ++i)
				if (quest.vars.count("event." + std::to_string(stage.id) + "." + std::to_string(i)))
					rules.insert(rules.end(), stage.events[i].spawns.begin(), stage.events[i].spawns.end());
		for (const SpawnRule& rule : rules)
		{
			const std::string made = "spawned." + rule.source, beaten = "beaten." + rule.source;
			if (!owed)
			{
				quest.vars[made] = rule.countMin + (rule.countMax > rule.countMin ? static_cast<int>(draw() % static_cast<uint64_t>(rule.countMax - rule.countMin + 1)) : 0);
				quest.vars[beaten] = 0;
				++m_revision;
			}
			// what is owed: made, not beaten, not standing (never made yet, or streamed out), made when the player is near
			int standing = 0;
			for (const auto& [ref, source] : quest.spawned) standing += source == rule.source ? 1 : 0;
			const int count = static_cast<int>(std::max(0.0, std::min(1000.0, quest.vars[made] - quest.vars[beaten] - standing)));
			auto at = quest.aliases.find(rule.at);
			const Ref where = at != quest.aliases.end() ? at->second : Ref{};
			const double near = rule.near > 0.0 ? rule.near : m_settings.spawnMeters;
			if (count > 0 && near > 0.0)
			{
				const double d = where.valid() ? m_binding.distance(where) : -1.0;
				if (d < 0.0 || d > near) continue;   // (far, or not known: later, when the player comes near)
			}
			for (int i = 0; i < count; ++i)
			{
				const Ref ref = m_binding.spawn(rule, where);
				// one the world could not make now (its cap, no room) stays owed, made again at a later look: never
				// counted beaten (an errand never won without its fight)
				if (ref.valid()) quest.spawned[ref] = rule.source;
				else break;
			}
		}
	}

	void QuestSystem::beat(Quest& quest, const Ref& ref)
	{
		auto found = quest.spawned.find(ref);
		if (found == quest.spawned.end()) return;   // (each counted once: it leaves the list)
		quest.vars["beaten." + found->second] += 1;
		quest.spawned.erase(found);
		++m_revision;
	}

	bool QuestSystem::cleared(const Quest& quest, const Stage& stage) const
	{
		std::vector<SpawnRule> rules = stage.spawns;
		for (size_t i = 0; i < stage.events.size(); ++i)
			if (quest.vars.count("event." + std::to_string(stage.id) + "." + std::to_string(i)))
				rules.insert(rules.end(), stage.events[i].spawns.begin(), stage.events[i].spawns.end());
		if (rules.empty()) return false;
		for (const SpawnRule& rule : rules)
		{
			auto made = quest.vars.find("spawned." + rule.source), beaten = quest.vars.find("beaten." + rule.source);
			if (made == quest.vars.end() || beaten == quest.vars.end() || beaten->second < made->second) return false;
		}
		return true;
	}

	void QuestSystem::protect(const Quest& quest, bool on)
	{
		const QuestRecord* record = this->record(quest.record);
		if (!record) return;
		for (const auto& [name, rule] : record->aliases)
		{
			auto held = quest.aliases.find(name);
			if (rule.protect && held != quest.aliases.end() && held->second.valid()) m_binding.protect(held->second, on);
		}
	}

	const SpawnRule* QuestSystem::spawn_rule(const Quest& quest, const std::string& source) const
	{
		const QuestRecord* record = this->record(quest.record);
		if (record)
			for (const Stage& stage : record->stages)
			{
				for (const SpawnRule& rule : stage.spawns) if (rule.source == source) return &rule;
				for (const TimedEvent& event : stage.events) for (const SpawnRule& rule : event.spawns) if (rule.source == source) return &rule;
			}
		return nullptr;
	}

	void QuestSystem::despawn_all(Quest& quest, bool now)
	{
		for (const auto& [ref, source] : quest.spawned)
		{
			const SpawnRule* rule = spawn_rule(quest, source);
			if (rule && rule->teardown == "release") continue;   // (left to the world)
			if (!m_binding.alive(ref)) continue;
			// never taken away in front of the player: near, it waits until the player is far
			const double d = m_settings.despawnMeters > 0.0 && !now ? m_binding.distance(ref) : -1.0;
			if (d >= 0.0 && d < m_settings.despawnMeters) m_despawnLater.push_back(ref);
			else m_binding.despawn(ref);
		}
		quest.spawned.clear();
	}

	void QuestSystem::step(uint64_t step, double seconds)
	{
		const auto began = std::chrono::steady_clock::now();
		struct Timed
		{
			QuestSystem& self; std::chrono::steady_clock::time_point began;
			~Timed()
			{
				const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
				if (self.m_stepMs.size() < kTimesKept) self.m_stepMs.push_back(ms);
				else self.m_stepMs[self.m_stepNext] = ms;
				self.m_stepNext = (self.m_stepNext + 1) % kTimesKept;
			}
		} timed{ *this, began };
		m_step = step;
		if (!m_despawnLater.empty() && step % kRefillSteps == 0)
		{
			std::vector<Ref> still;
			for (const Ref& ref : m_despawnLater)
			{
				if (!m_binding.alive(ref)) continue;
				const double d = m_binding.distance(ref);
				if (d >= 0.0 && d < m_settings.despawnMeters) still.push_back(ref);
				else m_binding.despawn(ref);
			}
			m_despawnLater.swap(still);
		}
		std::vector<Pending> pending;
		pending.swap(m_pending);
		for (const Pending& p : pending)
		{
			auto found = m_quests.find(p.serial);
			if (p.kind == kSignal) apply_signal(p.name, p.subject);
			else if (p.kind == kAbandon && found != m_quests.end()) end(found->second, "abandoned", "command");
			else if (p.kind == kFail && found != m_quests.end()) end(found->second, "failed", "command: " + p.reason);
			else if (p.kind == kForce && found != m_quests.end() && found->second.state == "active")
			{
				const QuestRecord* r = record(found->second.record);
				if (r && r->stage(p.stage)) transition(found->second, p.stage, "next", "qa");
			}
			else if (p.kind == kStart) start(p.name, p.reason);
			else if (p.kind == kUse)
			{
				// the use made, if it is still offered now (the same stage, the player still near): credited, and the exit taken
				// when its other conditions hold (they do: it was offered)
				const QuestRecord* r = found != m_quests.end() ? record(found->second.record) : nullptr;
				const Stage* st = r && found->second.state == "active" && found->second.stage == p.stage ? r->stage(p.stage) : nullptr;
				Offer offer;
				if (!st || p.exit < 0 || static_cast<size_t>(p.exit) >= st->exits.size() || !offered(found->second, *st, static_cast<size_t>(p.exit), &offer))
				{
					log_entry({ { "seq", m_nextSeq++ }, { "step", m_step }, { "serial", p.serial }, { "record", found != m_quests.end() ? found->second.record : std::string() },
						{ "from", p.stage }, { "to", p.stage }, { "outcome", "refused" }, { "cause", "use of exit " + std::to_string(p.exit) + ": not offered now" } });
					continue;
				}
				Quest& q = found->second;
				q.vars["use." + std::to_string(p.stage) + "." + std::to_string(p.exit)] = 1.0;
				++m_revision;
				take_exit(q, st->exits[static_cast<size_t>(p.exit)], "use:" + offer.alias + " (" + offer.action + ")");
			}
			else if (p.kind == kRestart && found != m_quests.end() && found->second.state == "active")
			{
				Quest& q = found->second;
				const QuestRecord* r = record(q.record);
				if (!r) continue;
				despawn_all(q);
				const int at = q.checkpoint && r->stage(q.checkpoint) ? q.checkpoint : r->stages.front().id;
				transition(q, at, "restart", "command");
				m_binding.restarted(q.serial, at);
			}
		}
		for (auto& [serial, quest] : m_quests)
		{
			if (quest.state != "active") continue;
			quest.elapsed += seconds;
			quest.total += seconds;
			const QuestRecord* record = this->record(quest.record);
			if (record && record->limitSeconds > 0.0 && quest.total >= record->limitSeconds)
			{
				quest.total = 0.0;   // (a story quest that retries gets its limit again)
				end(quest, "failed", "limit");
				continue;
			}
			// a guiding mark looked at again (every kRefillSteps for all quests: a find over the world is not free)
			if (record && (step + quest.serial) % kRefillSteps == 0)   // (spread by serial: each quest its own step of the 30)
				for (const auto& [name, rule] : record->aliases)
					if (rule.repick) { repick(quest, *record); break; }
			// an alias left empty by a retry: filled as soon as the world has a target for it
			// (looked for every kRefillSteps, not every step: a find over the world is not free)
			for (const auto& [name, ref] : quest.aliases)
				if (!ref.valid() && m_step >= quest.refillAt)
				{
					if (!refill_lost(quest)) quest.refillAt = m_step + kRefillSteps;
					break;
				}
			// a spawn no longer in the world: beaten when it was killed or destroyed without a signal; streamed or culled
			// out (the player walked away), owed: made again when the player comes near (never a hunt won by leaving)
			std::vector<Ref> gone;
			for (const auto& [ref, source] : quest.spawned) if (!m_binding.alive(ref)) gone.push_back(ref);
			for (const Ref& ref : gone)
			{
				if (m_binding.defeated(ref)) beat(quest, ref);
				else { quest.spawned.erase(ref); quest.owedAt = 0; }
			}
			if (m_step >= quest.owedAt)
			{
				const Stage* stage = record ? record->stage(quest.stage) : nullptr;
				if (stage && !stage->spawns.empty()) make_spawns(quest, *stage, true);
				quest.owedAt = m_step + kRefillSteps;
			}
			// an alias whose target is gone: emptied and filled again when its rule says "refill" (now or at a later
			// step), else the signal "alias_lost" with it as the subject
			for (auto& [name, ref] : quest.aliases)
			{
				if (!ref.valid() || m_binding.alive(ref)) continue;
				const AliasRule* rule = record && record->aliases.count(name) ? &record->aliases.at(name) : nullptr;
				if (rule && rule->repick) continue;   // (a guiding mark gone moves to another at its next look: never a loss)
				if (rule && rule->lost == "refill")
				{
					if (rule->protect) m_binding.protect(ref, false);   // (lost: no longer the quest's)
					ref = Ref{};
					refill_lost(quest);
				}
				else { const Ref lost = ref; apply_signal("alias_lost", lost); }   // (a copy: a retry may replace the aliases)
				break;   // (the aliases may have changed: one a step)
			}
			if (const Stage* now = record ? record->stage(quest.stage) : nullptr; now && !now->events.empty()) fire_events(quest, *now);
			if (const Stage* now = record ? record->stage(quest.stage) : nullptr) count_holds(quest, *now, seconds);
			if (quest.state != "active") continue;
			check_conditions(quest);
		}
	}

	bool QuestSystem::level_scoped(const Quest& quest) const
	{
		const QuestRecord* record = this->record(quest.record);
		return record && record->scope == "level";
	}

	void QuestSystem::end_level()
	{
		std::vector<uint64_t> gone;
		for (auto& [serial, quest] : m_quests)
		{
			if (!level_scoped(quest)) continue;
			if (quest.state == "active")
			{
				m_arbiter.release(quest.owner());
				despawn_all(quest, true);
				protect(quest, false);
				log_entry({ { "seq", m_nextSeq++ }, { "step", m_step }, { "serial", quest.serial }, { "record", quest.record },
					{ "from", quest.stage }, { "to", quest.stage }, { "outcome", "unfinished" }, { "cause", "the level ended" } });
				if (m_outcome) m_outcome(quest, "unfinished");
			}
			gone.push_back(serial);
		}
		for (uint64_t serial : gone) m_quests.erase(serial);
	}

	nlohmann::json QuestSystem::guide(uint64_t serial) const
	{
		if (serial == 0)
		{
			const std::string& owner = m_arbiter.presentation().owner;
			const size_t hash = owner.rfind('#');
			if (hash == std::string::npos) return nullptr;
			serial = std::strtoull(owner.c_str() + hash + 1, nullptr, 10);
		}
		auto found = m_quests.find(serial);
		if (found == m_quests.end() || found->second.state != "active") return nullptr;
		const Quest& quest = found->second;
		const QuestRecord* record = this->record(quest.record);
		const Stage* stage = record ? record->stage(quest.stage) : nullptr;
		if (!stage) return nullptr;
		nlohmann::json out = { { "serial", quest.serial }, { "record", quest.record }, { "stage", quest.stage }, { "marker", nullptr }, { "past", nullptr }, { "hold", nullptr } };
		auto marked = quest.aliases.find(stage->marker);
		if (!stage->marker.empty() && marked != quest.aliases.end() && marked->second.valid())
			out["marker"] = { { "kind", marked->second.kind }, { "id", marked->second.id } };
		for (size_t i = 0; i < stage->exits.size(); ++i)
		{
			const Exit& exit = stage->exits[i];
			if (!exit.pastAxis.empty() && out["past"].is_null()) out["past"] = { { "axis", exit.pastAxis }, { "value", exit.pastValue }, { "way", exit.pastWay } };
			if (exit.useMeters >= 0.0 && !out.contains("use") && (exit.needs.empty() || carries(exit.needs)))
			{
				auto target = quest.aliases.find(exit.useAlias);
				if (target != quest.aliases.end() && target->second.valid())
					out["use"] = { { "exit", i }, { "alias", exit.useAlias }, { "action", exit.useAction }, { "meters", exit.useMeters },
						{ "kind", target->second.kind }, { "id", target->second.id } };
			}
			if (exit.holdSeconds > 0.0 && out["hold"].is_null())
			{
				auto held = quest.vars.find("hold." + std::to_string(stage->id) + "." + std::to_string(i));
				out["hold"] = { { "seconds", exit.holdSeconds }, { "held", held != quest.vars.end() ? held->second : 0.0 } };
			}
		}
		return out;
	}

	nlohmann::json QuestSystem::journal() const
	{
		nlohmann::json out = nlohmann::json::array();
		for (auto it = m_quests.rbegin(); it != m_quests.rend(); ++it)
		{
			const Quest& q = it->second;
			const QuestRecord* record = this->record(q.record);
			const Stage* stage = record ? record->stage(q.stage) : nullptr;
			// what a running quest's stage waits on, one line per exit (a stuck quest read from a tool)
			nlohmann::json waits = nlohmann::json::array();
			if (stage && q.state == "active")
				for (const Exit& exit : stage->exits) waits.push_back(describe_exit(exit) + " -> " + std::to_string(exit.to));
			out.push_back({ { "serial", q.serial }, { "record", q.record }, { "title", record ? record->title : std::string() }, { "state", q.state },
				{ "stage", q.stage }, { "objectives", stage && q.state == "active" ? stage->objectives : std::vector<std::string>() }, { "waits", waits } });
		}
		return out;
	}

	nlohmann::json QuestSystem::save() const
	{
		nlohmann::json quests = nlohmann::json::object(), rewarded = nlohmann::json::array(), done = nlohmann::json::object();
		for (const auto& [record, n] : m_done) done[record] = n;
		for (const auto& [serial, q] : m_quests)
		{
			if (q.state == "broken") { quests[std::to_string(serial)] = q.kept; continue; }
			if (q.state != "active" || level_scoped(q)) continue;   // (a level quest is never saved)
			nlohmann::json aliases = nlohmann::json::object(), counted = nlohmann::json::object();
			for (const auto& [name, ref] : q.aliases) aliases[name] = m_binding.persist(ref);
			for (const auto& [key, subjects] : q.counted) counted[key] = subjects;
			quests[std::to_string(serial)] = { { "record", q.record }, { "version", q.version }, { "stage", q.stage }, { "aliases", aliases },
				{ "vars", q.vars }, { "counted", counted }, { "elapsed", q.elapsed }, { "total", q.total }, { "checkpoint", q.checkpoint } };
		}
		for (const auto& [serial, reward] : m_rewarded) rewarded.push_back({ { "serial", serial }, { "reward", reward } });
		nlohmann::json out = { { "format", 1 }, { "nextSerial", m_nextSerial }, { "draws", m_draws }, { "done", done }, { "quests", quests }, { "rewarded", rewarded } };
		// what waits for the next step and is the world's or a record's own, not a tester's: a 'then' or named 'onFail' (the
		// quest that queued it is done already: lost, its chain would stop there) and a signal (a timed event fired, the
		// game's own); a save between the two steps keeps them
		nlohmann::json pending = nlohmann::json::array();
		for (const Pending& p : m_pending)
		{
			if (p.kind == kStart) pending.push_back({ { "start", p.name }, { "reason", p.reason } });
			else if (p.kind == kSignal)
			{
				const std::string key = p.subject.valid() ? m_binding.persist(p.subject) : std::string();
				if (!p.subject.valid() || !key.empty()) pending.push_back({ { "signal", p.name }, { "subject", key } });
			}
		}
		if (!pending.empty()) out["pending"] = pending;
		if (!m_items.empty()) out["items"] = m_items;
		return out;
	}

	bool QuestSystem::load(const nlohmann::json& saved, std::vector<std::string>& errors)
	{
		try { return load_checked(saved, errors); }
		catch (const std::exception& e)   // (no path above should throw; a damaged save still never takes the game down)
		{
			errors.push_back(std::string("quests: the save could not be read: ") + e.what());
			return false;
		}
	}

	namespace
	{
		// a whole number from 0 to 'most' (held signed in memory, unsigned once parsed from text: either)
		bool is_count(const nlohmann::json& j, uint64_t most)
		{
			if (j.is_number_unsigned()) return j.get<uint64_t>() <= most;
			return j.is_number_integer() && j.get<int64_t>() >= 0 && static_cast<uint64_t>(j.get<int64_t>()) <= most;
		}
		bool is_amount(const nlohmann::json& j, double low, double high)
		{
			if (!j.is_number()) return false;
			const double v = j.get<double>();
			return std::isfinite(v) && v >= low && v <= high;
		}
		bool is_serial_key(const std::string& key)
		{
			if (key.empty() || key.size() > 15 || key[0] == '0') return false;
			for (char c : key) if (c < '0' || c > '9') return false;
			return true;
		}
	}

	bool QuestSystem::load_checked(const nlohmann::json& saved, std::vector<std::string>& errors)
	{
		constexpr uint64_t kMostSerial = 1000000000000000ull;   // (15 digits: far beyond any game, below every overflow)
		constexpr double kMostSeconds = 1e9, kMostVar = 1e15;
		const size_t before = errors.size();
		// the block itself: what it is wrong, nothing is touched (the game keeps the saved block as it is)
		if (!saved.is_object()) { errors.push_back("quests: the saved block is not an object"); return false; }
		if (!saved.contains("format") || !saved["format"].is_number_integer() || saved["format"].get<int64_t>() != 1)
		{
			errors.push_back("quests: a save of an unknown format (" + (saved.contains("format") ? saved["format"].dump() : std::string("none")) + ")");
			return false;
		}
		const nlohmann::json none = nlohmann::json::object();
		const nlohmann::json& doneJson = saved.contains("done") ? saved["done"] : none;
		const nlohmann::json& questsJson = saved.contains("quests") ? saved["quests"] : none;
		const nlohmann::json emptyList = nlohmann::json::array();
		const nlohmann::json& rewardedJson = saved.contains("rewarded") ? saved["rewarded"] : emptyList;
		if (!doneJson.is_object() || !questsJson.is_object() || !rewardedJson.is_array())
		{
			errors.push_back("quests: the saved block's done, quests or rewarded is of the wrong type");
			return false;
		}
		uint64_t nextSerial = 1, draws = 0;
		if (saved.contains("nextSerial"))
		{
			if (!is_count(saved["nextSerial"], kMostSerial)) { errors.push_back("quests: nextSerial is not a serial"); return false; }
			nextSerial = std::max<uint64_t>(1, saved["nextSerial"].get<uint64_t>());
		}
		if (saved.contains("draws"))
		{
			if (!is_count(saved["draws"], UINT64_MAX)) { errors.push_back("quests: draws is not a count"); return false; }
			draws = saved["draws"].get<uint64_t>();
		}
		// the player's quest items: a bad entry left out
		std::map<std::string, int> itemsLoaded;
		if (saved.contains("items"))
		{
			if (!saved["items"].is_object()) { errors.push_back("quests: items is not an object"); return false; }
			for (auto it = saved["items"].begin(); it != saved["items"].end(); ++it)
			{
				if (it.key().empty() || !is_count(it.value(), 1000000) || it.value().get<int64_t>() == 0) { errors.push_back("quests: the item '" + it.key() + "' is not a count: left out"); continue; }
				itemsLoaded[it.key()] = static_cast<int>(it.value().get<int64_t>());
			}
		}
		// what waited for the next step (a 'then', an 'onFail', a signal): a bad entry left out
		std::vector<Pending> pendingLoaded;
		if (saved.contains("pending"))
		{
			if (!saved["pending"].is_array()) { errors.push_back("quests: pending is not a list"); return false; }
			for (const nlohmann::json& p : saved["pending"])
			{
				if (p.is_object() && p.contains("start") && p["start"].is_string())
					pendingLoaded.push_back({ kStart, p["start"].get<std::string>(), {}, 0, p.contains("reason") && p["reason"].is_string() ? p["reason"].get<std::string>() : std::string("a saved follow-up") });
				else if (p.is_object() && p.contains("signal") && p["signal"].is_string())
				{
					const std::string key = p.contains("subject") && p["subject"].is_string() ? p["subject"].get<std::string>() : std::string();
					const Ref subject = key.empty() ? Ref{} : m_binding.restore(key);
					if (!key.empty() && !subject.valid()) { errors.push_back("quests: a pending signal's subject '" + key + "' is gone: left out"); continue; }
					pendingLoaded.push_back({ kSignal, p["signal"].get<std::string>(), subject, 0, {} });
				}
				else errors.push_back("quests: a pending entry is neither a start nor a signal: left out");
			}
		}
		// what it holds, one entry at a time: a bad one is left out (a line each), the rest kept
		std::map<std::string, int> done;
		for (auto it = doneJson.begin(); it != doneJson.end(); ++it)
		{
			if (!is_count(it.value(), 1000000)) { errors.push_back("quests: done '" + it.key() + "' is not a count: left out"); continue; }
			done[it.key()] = static_cast<int>(it.value().get<uint64_t>());
		}
		std::map<uint64_t, Quest> quests;
		std::set<Ref> held;
		for (auto it = questsJson.begin(); it != questsJson.end(); ++it)
		{
			const std::string where = "quests: the saved quest '" + it.key() + "'";
			auto bad = [&](const std::string& why) { errors.push_back(where + " " + why + ": left out"); };
			const nlohmann::json& s = it.value();
			if (!is_serial_key(it.key())) { bad("has no serial for a key"); continue; }
			if (!s.is_object()) { bad("is not an object"); continue; }
			Quest q;
			q.serial = std::stoull(it.key());
			if (!s.contains("record") || !s["record"].is_string()) { bad("names no record"); continue; }
			q.record = s["record"].get<std::string>();
			if (!s.contains("stage") || !s["stage"].is_number_integer()) { bad("has no stage"); continue; }
			const QuestRecord* record = this->record(q.record);
			const int64_t stage = s["stage"].get<int64_t>();
			const Stage* at = record && stage >= INT32_MIN && stage <= INT32_MAX ? record->stage(static_cast<int>(stage)) : nullptr;
			if (!record || !at || !at->end.empty())
			{
				// its record or its stage is gone (a build without it): kept as "broken", visible, never stepped, saved back
				// as it was, so the build that has it plays it on (a story quest is never silently lost)
				q.state = "broken";
				q.kept = s;
				errors.push_back(where + (record ? " is at a stage its record has not" : " has a record this build has not ('" + q.record + "')") + ": kept as broken");
				nextSerial = std::max(nextSerial, q.serial + 1);
				quests[q.serial] = q;
				continue;
			}
			q.stage = static_cast<int>(stage);
			q.version = s.contains("version") && s["version"].is_number_integer() ? static_cast<int>(s["version"].get<int64_t>()) : record->version;
			const nlohmann::json& aliases = s.contains("aliases") ? s["aliases"] : none;
			const nlohmann::json& vars = s.contains("vars") ? s["vars"] : none;
			const nlohmann::json& counted = s.contains("counted") ? s["counted"] : none;
			if (!aliases.is_object() || !vars.is_object() || !counted.is_object()) { bad("has aliases, vars or counted of the wrong type"); continue; }
			bool ok = true;
			for (auto a = aliases.begin(); a != aliases.end() && ok; ++a)
			{
				if (!a.value().is_string()) { ok = false; break; }
				if (!record->aliases.count(a.key())) continue;   // (an alias the record no longer has)
				const Ref ref = m_binding.restore(a.value().get<std::string>());
				if (ref.valid() && record->aliases.at(a.key()).reserve && !held.insert(ref).second) { ok = false; break; }   // (two quests, one target)
				q.aliases[a.key()] = ref;
			}
			for (auto v = vars.begin(); v != vars.end() && ok; ++v)
			{
				if (!is_amount(v.value(), -kMostVar, kMostVar)) ok = false;
				else q.vars[v.key()] = v.value().get<double>();
			}
			for (auto c = counted.begin(); c != counted.end() && ok; ++c)
			{
				if (!c.value().is_array()) { ok = false; break; }
				std::set<std::string>& subjects = q.counted[c.key()];
				for (const nlohmann::json& subject : c.value())
				{
					if (!subject.is_string()) { ok = false; break; }
					subjects.insert(subject.get<std::string>());
				}
			}
			if (!ok) { bad("has an alias, a var or a count that is not one (or a target another saved quest holds)"); continue; }
			for (const char* field : { "elapsed", "total" })
				if (s.contains(field) && !is_amount(s[field], 0.0, kMostSeconds)) ok = false;
			if (!ok) { bad("has a time that is not one"); continue; }
			q.elapsed = s.contains("elapsed") ? s["elapsed"].get<double>() : 0.0;
			q.total = s.contains("total") ? s["total"].get<double>() : 0.0;
			// its last checkpoint: a stage its record has (else none: it restarts from its start)
			if (s.contains("checkpoint") && s["checkpoint"].is_number_integer())
			{
				const int64_t c = s["checkpoint"].get<int64_t>();
				const Stage* cs = c > 0 && c <= INT32_MAX ? record->stage(static_cast<int>(c)) : nullptr;
				q.checkpoint = cs && cs->checkpoint ? static_cast<int>(c) : 0;
			}
			// an alias the save lacks: empty, filled again at a step (as after a retry)
			for (const auto& [name, rule] : record->aliases) if (!q.aliases.count(name)) q.aliases[name] = Ref{};
			nextSerial = std::max(nextSerial, q.serial + 1);
			quests[q.serial] = q;
		}
		std::vector<std::pair<uint64_t, std::string>> rewarded;
		for (const nlohmann::json& r : rewardedJson)
		{
			if (!r.is_object() || !r.contains("serial") || !is_count(r["serial"], kMostSerial) || !r.contains("reward") || !r["reward"].is_string())
			{
				errors.push_back("quests: a saved reward that is not one: left out");
				continue;
			}
			rewarded.push_back({ r["serial"].get<uint64_t>(), r["reward"].get<std::string>() });
			nextSerial = std::max(nextSerial, rewarded.back().first + 1);
		}
		while (rewarded.size() > kRewardsKept) rewarded.erase(rewarded.begin());

		// all read: now the state is replaced (the running quests' claims released, the loaded ones' made)
		for (auto& [serial, q] : m_quests)
			if (q.state == "active") { m_arbiter.release(q.owner()); despawn_all(q, true); protect(q, false); }   // (replaced: its spawns go at once, never doubled by the loaded ones)
		for (const Ref& ref : m_despawnLater) if (m_binding.alive(ref)) m_binding.despawn(ref);
		m_despawnLater.clear();   // (a load replacing the state: the removals waiting go at once, like the old quests' spawns)
		m_quests = std::move(quests);
		m_done = std::move(done);
		m_pending = std::move(pendingLoaded);   // (a tester's moves waiting are dropped: what was saved is what plays)
		m_items = std::move(itemsLoaded);
		m_nextSerial = nextSerial;
		m_draws = draws;
		++m_revision;
		for (auto& [serial, q] : m_quests)
		{
			if (q.state == "broken")
				log_entry({ { "seq", m_nextSeq++ }, { "step", m_step }, { "serial", q.serial }, { "record", q.record },
					{ "from", q.kept.value("stage", 0) }, { "to", q.kept.value("stage", 0) }, { "outcome", "broken" }, { "cause", "load: its record or stage is not in this build" } });
			if (q.state != "active") continue;
			protect(q, true);
			if (const QuestRecord* record = this->record(q.record))
				if (const Stage* stage = record->stage(q.stage)) make_spawns(q, *stage, true);   // (only what is still owed)
			claim(q);
		}
		// every reward a quest earned is offered again: the game grants the serials its own save lacks (a kill between
		// the two writes, either way round)
		m_rewarded = std::move(rewarded);
		for (const auto& [serial, reward] : m_rewarded) if (m_rewardOffer) m_rewardOffer(serial, reward);
		(void)before;
		return true;
	}
}
