// The quests' arbiter (docs/QUESTS.md, section 8): one claim shown, its marker and its lines together, by the tracked
// owner, priority and claim order; the presentation's revision; the state and the log the tools read; the tools'
// safety (a call in flight while the arbiter goes; none in a release). No window, no genre.
#include "quests/QuestArbiter.h"
#include "quests/QuestTools.h"
#include "mcp/McpServer.h"

#include <atomic>
#include <cstdio>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace Pakal::Quests;

namespace
{
	int failures = 0;
	void check(bool condition, const char* what)
	{
		std::printf("%s %s\n", condition ? "ok  " : "FAIL", what);
		if (!condition) ++failures;
	}

	Claim showing(int priority, uint64_t point, const std::string& text = std::string())
	{
		Claim claim;
		claim.priority = priority;
		if (point) claim.marker = Target{ "point", point };
		if (!text.empty()) claim.objectives.push_back({ text });
		return claim;
	}

	std::string marker_owner(const Presentation& p) { return p.marker ? p.marker->owner : std::string(); }
	uint64_t point_of(const Presentation& p) { return p.marker ? p.marker->target.id : 0; }
	std::string line_of(const Presentation& p) { return p.objectives.empty() ? std::string() : p.objectives[0].text; }

	std::string call(Pakal::McpServer& server, const std::string& tool, const std::string& arguments = "{}")
	{
		return server.handle_message("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"" + tool
			+ "\",\"arguments\":" + arguments + "}}");
	}
}

int main()
{
	// nothing claimed: nothing shown
	{
		Arbiter a;
		const Presentation& p = a.resolve(1);
		check(!p.marker && p.objectives.empty() && p.secondary.empty() && p.owner.empty(), "no claim: no marker, no line");
	}
	// the higher priority is shown, its marker and its line; the other's are not shown at all
	{
		Arbiter a;
		a.claim("story", showing(40, 7, "OBJ_STORY"));
		a.claim("errand", showing(50, 12, "OBJ_ERRAND"));
		const Presentation& p = a.resolve(1);
		check(p.owner == "errand" && marker_owner(p) == "errand" && point_of(p) == 12 && line_of(p) == "OBJ_ERRAND" && p.secondary.empty(),
			"two claims: the higher priority's marker and line, only them");
		const uint32_t revision = p.revision;
		check(a.resolve(2).revision == revision, "a step with no change keeps the revision");
		a.release("errand");
		const Presentation& after = a.resolve(3);
		check(after.owner == "story" && point_of(after) == 7 && after.revision == revision + 1, "released: the next owner's, one revision on");
	}
	// the arrow and the line never come from two owners (QA B1, the lead's run): a claim with a marker and no line wins
	// over one with both, and then no line is shown; a claim with a line and no marker wins, and then no marker
	{
		Arbiter a;
		a.claim("errand", showing(50, 1024, "OBJ_MISSION_RETRIEVE"));
		a.claim("outpost", showing(90, 400));
		const Presentation& p = a.resolve(1);
		check(p.owner == "outpost" && point_of(p) == 400 && p.objectives.empty(), "a marker without a line wins: no other owner's line beside its arrow");
		Arbiter b;
		b.claim("guide", showing(10, 400, "OBJ_WORLD_FREE"));
		b.claim("story", showing(40, 0, "OBJ_STORY_NOTES"));
		const Presentation& q = b.resolve(1);
		check(q.owner == "story" && line_of(q) == "OBJ_STORY_NOTES" && !q.marker, "a line without a marker wins: no other owner's arrow beside its line");
	}
	// a tie goes to the earlier claim; claimed again after a release, it queues behind
	{
		Arbiter a;
		a.claim("job", showing(50, 3, "J"));
		a.claim("errand", showing(50, 4, "E"));
		check(a.resolve(1).owner == "job", "a tie: the earlier claim");
		a.release("job");
		a.claim("job", showing(50, 3, "J"));
		check(a.resolve(2).owner == "errand", "claimed again after a release: it queues behind");
	}
	// the tracked owner wins whatever its priority while it shows something; its secondary marks show with it, and also
	// when it only has secondary marks (it does not win then)
	{
		Arbiter a;
		Claim guide = showing(10, 1, "G");
		guide.secondary = { Target{ "point", 2 }, Target{ "prop", 3 } };
		a.claim("guide", guide);
		a.claim("defense", showing(90, 9, "D"));
		check(a.resolve(1).owner == "defense" && a.presentation().secondary.empty(), "untracked: the defense is shown, no secondary marks");
		a.set_tracked("guide");
		const Presentation& p = a.resolve(2);
		check(p.owner == "guide" && p.tracked == "guide" && p.secondary.size() == 2 && p.secondary[1].target.kind == "prop",
			"tracked: its marker, its line and its secondary marks");
		a.release("guide");
		check(a.resolve(3).owner == "defense" && a.presentation().tracked.empty(), "the tracked owner gone: back to priority");
		Claim escort;
		escort.priority = 50;
		escort.secondary = { Target{ "actor", 77 } };
		a.claim("escort", escort);
		a.set_tracked("escort");
		const Presentation& e = a.resolve(4);
		check(e.owner == "defense" && e.secondary.size() == 1 && e.secondary[0].owner == "escort",
			"a tracked claim with only secondary marks: the defense is shown, the tracked one's marks too");
	}
	// the case that misled the player: a story mark and an errand on the same point
	{
		Arbiter a;
		a.claim("story", showing(40, 1013, "OBJ_STORY_SCAVENGE"));
		a.claim("errand", showing(50, 1013, "OBJ_MISSION_RETRIEVE"));
		check(a.resolve(1).owner == "errand" && line_of(a.presentation()) == "OBJ_MISSION_RETRIEVE", "one point claimed twice: the errand's");
		a.release("errand");
		const Presentation& after = a.resolve(2);
		check(after.owner == "story" && point_of(after) == 1013 && line_of(after) == "OBJ_STORY_SCAVENGE", "the errand done: the point stays marked, as the story's");
	}
	// the state and the log the tools read; the QUEST lines; a step's claims logged with it
	{
		Arbiter a;
		std::vector<std::string> lines;
		a.set_log_line([&lines](const std::string& line) { lines.push_back(line); });
		a.begin_step(7);
		a.claim("errand", showing(50, 12, "OBJ_ERRAND"));
		a.resolve(7);
		const nlohmann::json state = a.state();
		check(state["claims"].contains("errand") && state["claims"]["errand"]["priority"] == 50 && state["presentation"]["owner"] == "errand"
			&& state["step"] == 7, "quests.state: the claims, the presentation, the step");
		const nlohmann::json log = a.log_since(0);
		check(log.size() == 2 && log[0]["event"] == "claim" && log[0]["step"] == 7 && log[1]["event"] == "shown", "quests.log: the claim (its step), what is shown");
		check(a.log_since(log[0]["seq"].get<uint64_t>()).size() == 1, "quests.log since a sequence number");
		check(lines.size() == 2 && lines[0].rfind("QUEST {", 0) == 0, "every decision is a QUEST line");
		a.claim("errand", showing(50, 12, "OBJ_ERRAND"));   // the same claim again
		a.resolve(8);
		check(a.log_since(0).size() == 2, "the same claim again logs nothing");
		check(a.published() == a.presentation(), "the published copy equals the presentation");
		a.publish_game({ { "story", 2 } });
		check(a.state()["game"]["story"] == 2, "the game's own part is in the state");
	}
	// seeded random claims (markers and lines each present or not), releases and tracking: at every step the shown owner
	// is the best-ranked claim that shows anything, and the marker and the lines are both that owner's
	{
		std::mt19937 random(20261122u);
		Arbiter a;
		const std::vector<std::string> owners = { "story", "errand", "job", "outpost:a", "outpost:b", "guide", "challenge" };
		std::map<std::string, Claim> live;
		std::map<std::string, uint64_t> claimedAt;
		uint64_t order = 0;
		std::string tracked;
		bool held = true;
		for (int step = 1; step <= 10000 && held; ++step)
		{
			a.begin_step(static_cast<uint64_t>(step));
			const std::string owner = owners[random() % owners.size()];
			const int what = static_cast<int>(random() % 10);
			if (what < 5)
			{
				Claim c;
				c.priority = static_cast<int>(random() % 4) * 20 + 10;
				if (random() % 3) c.marker = Target{ "point", 1000 + random() % 30 };
				if (random() % 3) c.objectives.push_back({ "L" + std::to_string(random() % 5) });
				a.claim(owner, c);
				if (!live.count(owner)) claimedAt[owner] = order++;
				live[owner] = c;
			}
			else if (what < 8) { a.release(owner); live.erase(owner); }
			else { tracked = random() % 3 ? owner : std::string(); a.set_tracked(tracked); }
			const Presentation& p = a.resolve(static_cast<uint64_t>(step));
			std::string best;
			for (const auto& [o, c] : live)
			{
				if (!c.shows()) continue;
				if (best.empty()) { best = o; continue; }
				const bool ot = o == tracked, bt = best == tracked;
				if (ot != bt) { if (ot) best = o; continue; }
				if (c.priority != live[best].priority) { if (c.priority > live[best].priority) best = o; continue; }
				if (claimedAt[o] < claimedAt[best]) best = o;
			}
			const bool oneOwner = (!p.marker || p.marker->owner == p.owner) && (p.objectives.empty() || p.owner == best);
			held = p.owner == best && oneOwner && (best.empty() || (p.marker.has_value() == live[best].marker.has_value()
				&& p.objectives == live[best].objectives));
		}
		check(held, "10,000 seeded steps: the best-ranked claim is shown, its marker and lines together, never another's");
	}
	// the tools: never in a release, nor without a token; a call in flight while the arbiter goes reads a live copy
	{
		Pakal::McpServer server;
		auto arbiter = std::make_unique<Arbiter>();
		check(!add_tools(server, *arbiter, "token", "1") && call(server, "quests.state").find("unknown tool") != std::string::npos,
			"a release (PAKAL_RELEASE=1): no quests tools");
		check(!add_tools(server, *arbiter, nullptr, nullptr), "no token: no quests tools");
		check(add_tools(server, *arbiter, "token", nullptr) && call(server, "quests.state").find("\"isError\":false") != std::string::npos,
			"a development server with a token: the quests tools answer");
		arbiter->claim("errand", showing(50, 12, "OBJ_ERRAND"));
		arbiter->resolve(1);
		std::atomic<bool> stop{ false };
		std::atomic<int> answered{ 0 }, broken{ 0 };
		std::thread poller([&]()
		{
			while (!stop)
			{
				const std::string reply = call(server, "quests.state");
				if (reply.find("\"jsonrpc\"") == std::string::npos) ++broken;
				++answered;
			}
		});
		while (answered < 50) std::this_thread::yield();
		arbiter.reset();              // the level unloads: its arbiter goes first...
		const int afterReset = answered;
		while (answered < afterReset + 50) std::this_thread::yield();
		remove_tools(server);         // ...then the tools; calls still in flight read the shared copy
		stop = true;
		poller.join();
		check(broken == 0 && answered > 100, "quests.state called in a loop while the arbiter is destroyed: every call answered");
	}
	std::printf("%s\n", failures ? "SOME CHECKS FAILED" : "all checks passed");
	return failures ? 1 : 0;
}
