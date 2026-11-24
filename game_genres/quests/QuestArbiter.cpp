#include "quests/QuestArbiter.h"

#include <algorithm>

namespace Pakal::Quests
{
	nlohmann::json to_json(const Target& target)
	{
		return { { "kind", target.kind }, { "id", target.id } };
	}

	namespace
	{
		nlohmann::json lines_json(const std::vector<ObjectiveLine>& lines)
		{
			nlohmann::json out = nlohmann::json::array();
			for (const ObjectiveLine& line : lines) out.push_back({ { "text", line.text }, { "values", line.values } });
			return out;
		}

		nlohmann::json claim_json(const Claim& claim)
		{
			nlohmann::json out = { { "priority", claim.priority }, { "objectives", lines_json(claim.objectives) } };
			out["marker"] = claim.marker ? to_json(*claim.marker) : nlohmann::json();
			nlohmann::json secondary = nlohmann::json::array();
			for (const Target& target : claim.secondary) secondary.push_back(to_json(target));
			out["secondary"] = secondary;
			return out;
		}
	}

	nlohmann::json to_json(const Presentation& presentation)
	{
		nlohmann::json out = { { "revision", presentation.revision }, { "owner", presentation.owner }, { "tracked", presentation.tracked },
			{ "objectives", lines_json(presentation.objectives) } };
		out["marker"] = presentation.marker
			? nlohmann::json{ { "owner", presentation.marker->owner }, { "target", to_json(presentation.marker->target) } }
			: nlohmann::json();
		nlohmann::json secondary = nlohmann::json::array();
		for (const Marker& marker : presentation.secondary) secondary.push_back({ { "owner", marker.owner }, { "target", to_json(marker.target) } });
		out["secondary"] = secondary;
		return out;
	}

	nlohmann::json state_of(const Published& shared)
	{
		std::lock_guard<std::mutex> lock(shared.mutex);
		nlohmann::json out = shared.state;
		out["game"] = shared.game;
		return out;
	}

	nlohmann::json log_of(const Published& shared, uint64_t since)
	{
		std::lock_guard<std::mutex> lock(shared.mutex);
		nlohmann::json out = nlohmann::json::array();
		for (const nlohmann::json& entry : shared.log)
		{
			if (entry["seq"].get<uint64_t>() > since) out.push_back(entry);
		}
		return out;
	}

	void Arbiter::claim(const std::string& owner, const Claim& claim)
	{
		auto found = m_claims.find(owner);
		if (found != m_claims.end() && found->second == claim) return;
		if (!m_order.count(owner)) m_order[owner] = m_nextOrder++;
		m_claims[owner] = claim;
		record("claim", owner, claim_json(claim));
	}

	void Arbiter::release(const std::string& owner)
	{
		if (!m_claims.erase(owner)) return;
		m_order.erase(owner);   // (claimed again later, it queues behind the ones already there)
		record("release", owner, nlohmann::json::object());
	}

	const Claim* Arbiter::claim_of(const std::string& owner) const
	{
		auto found = m_claims.find(owner);
		return found != m_claims.end() ? &found->second : nullptr;
	}

	void Arbiter::set_tracked(const std::string& owner)
	{
		if (owner == m_tracked) return;
		m_tracked = owner;
		record("track", owner, nlohmann::json::object());
	}

	const Presentation& Arbiter::resolve(uint64_t step)
	{
		m_step = step;
		// the claims in the order they win: the tracked owner, then priority (higher first), then the earlier claim
		const std::pair<const std::string, Claim>* winner = nullptr;
		auto better = [this](const std::pair<const std::string, Claim>& a, const std::pair<const std::string, Claim>& b)
		{
			const bool at = a.first == m_tracked, bt = b.first == m_tracked;
			if (at != bt) return at;
			if (a.second.priority != b.second.priority) return a.second.priority > b.second.priority;
			return m_order.at(a.first) < m_order.at(b.first);
		};
		for (const auto& entry : m_claims)
		{
			if (!entry.second.shows()) continue;   // (a claim with only secondary marks never wins)
			if (!winner || better(entry, *winner)) winner = &entry;
		}
		// one owner shows both: its marker (if any) and its lines (if any), never another quest's beside them
		Presentation next;
		next.tracked = m_claims.count(m_tracked) ? m_tracked : std::string();
		if (winner)
		{
			next.owner = winner->first;
			if (winner->second.marker) next.marker = Marker{ *winner->second.marker, winner->first };
			next.objectives = winner->second.objectives;
		}
		// the secondary marks: the tracked claim's, whether or not it wins; else the winner's
		const std::string secondaryOwner = !next.tracked.empty() ? next.tracked : next.owner;
		if (!secondaryOwner.empty())
			for (const Target& target : m_claims.at(secondaryOwner).secondary) next.secondary.push_back({ target, secondaryOwner });
		if (!(next == m_presentation))
		{
			const bool ownerChanged = next.owner != m_presentation.owner || !(next.marker == m_presentation.marker)
				|| !(next.objectives == m_presentation.objectives);
			next.revision = m_presentation.revision + 1;
			m_presentation = next;
			if (ownerChanged)
			{
				nlohmann::json detail = { { "marker", m_presentation.marker ? to_json(m_presentation.marker->target) : nlohmann::json() },
					{ "objectives", lines_json(m_presentation.objectives) } };
				record("shown", m_presentation.owner, detail);
			}
		}
		if (m_changed) publish();   // (only after a change: a quiet step costs no copy)
		m_changed = false;
		return m_presentation;
	}

	Presentation Arbiter::published() const
	{
		std::lock_guard<std::mutex> lock(m_shared->mutex);
		return m_shared->presentation;
	}

	nlohmann::json Arbiter::state() const { return state_of(*m_shared); }
	nlohmann::json Arbiter::log_since(uint64_t seq) const { return log_of(*m_shared, seq); }

	void Arbiter::publish_game(nlohmann::json game)
	{
		std::lock_guard<std::mutex> lock(m_shared->mutex);
		m_shared->game = std::move(game);
	}

	void Arbiter::record(const std::string& event, const std::string& owner, const nlohmann::json& detail)
	{
		m_changed = true;
		nlohmann::json entry;
		{
			std::lock_guard<std::mutex> lock(m_shared->mutex);
			entry = { { "seq", m_shared->nextSeq++ }, { "step", m_step }, { "event", event }, { "owner", owner }, { "detail", detail } };
			m_shared->log.push_back(entry);
			while (m_shared->log.size() > Published::kLogKept) m_shared->log.pop_front();
		}
		if (m_logLine) m_logLine("QUEST " + entry.dump());
	}

	void Arbiter::publish()
	{
		nlohmann::json claims = nlohmann::json::object();
		for (const auto& [owner, claim] : m_claims) claims[owner] = claim_json(claim);
		nlohmann::json state = { { "step", m_step }, { "tracked", m_tracked }, { "claims", claims }, { "presentation", to_json(m_presentation) } };
		std::lock_guard<std::mutex> lock(m_shared->mutex);
		m_shared->presentation = m_presentation;
		m_shared->state = std::move(state);
	}
}
