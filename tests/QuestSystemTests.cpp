// The quests as they run (docs/QUESTS.md; the studio's MIS-V1..V4, T1..T5b): records read and checked, each
// transition, signals counted once per subject, a story that never stops, saves and loads mid-stage, rewards keyed by
// serial across a kill between two writes either way, and a seeded fuzz of 10,000 sequences against the invariants.
// A fake world: places with ids, a distance each, alive or not. No window, no genre.
#include "quests/QuestSystem.h"
#include "quests/QuestCommands.h"
#include "mcp/McpServer.h"

#include <atomic>
#include <thread>

#include <cstdio>
#include <cstdlib>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

using namespace Pakal::Quests;
using Json = nlohmann::json;

namespace
{
	int failures = 0;
	void check(bool condition, const char* what)
	{
		std::printf("%s %s\n", condition ? "ok  " : "FAIL", what);
		std::fflush(stdout);   // (a crash after it still shows the last check passed)
		if (!condition) ++failures;
	}

	// The fake world: places of a type, each with a distance from the player, alive or gone
	struct FakeBinding : Binding, TesterBinding
	{
		TesterBinding* tester() override { return this; }
		struct Place { std::string type; double distance = 100.0; bool alive = true; bool culled = false; };
		std::map<uint64_t, Place> places;
		void find(const AliasRule& rule, std::vector<Ref>& out) const override
		{
			for (const auto& [id, p] : places) if (p.type == rule.type && p.alive) out.push_back({ "place", id });
		}
		bool alive(const Ref& ref) const override { auto f = places.find(ref.id); return f != places.end() && f->second.alive && !f->second.culled; }
		bool defeated(const Ref& ref) const override { auto f = places.find(ref.id); return f == places.end() || !f->second.alive; }
		double distance(const Ref& ref) const override { auto f = places.find(ref.id); return f != places.end() ? f->second.distance : -1.0; }
		std::string persist(const Ref& ref) const override { return ref.valid() ? "place:" + std::to_string(ref.id) : std::string(); }
		Ref restore(const std::string& key) const override
		{
			if (key.rfind("place:", 0) != 0) return {};
			return { "place", std::stoull(key.substr(6)) };
		}
		// what a stage spawns is a new place of the rule's type; facts and protection kept as sets
		uint64_t nextSpawn = 100;
		int despawned = 0;
		std::set<uint64_t> protectedIds;
		std::set<std::pair<std::string, uint64_t>> facts;
		int refuseSpawns = 0;   // (the world at its cap: the next n spawns cannot be made)
		Ref spawn(const SpawnRule& rule, const Ref&) override { if (refuseSpawns > 0) { --refuseSpawns; return {}; } places[nextSpawn] = { rule.type, 20.0 }; return { "place", nextSpawn++ }; }
		void despawn(const Ref& ref) override { places[ref.id].alive = false; ++despawned; }
		Ref broughtTo; double broughtMeters = -1.0;
		bool bring_player(const Ref& ref, double meters) override { broughtTo = ref; broughtMeters = meters; return places.count(ref.id) > 0; }
		bool kill(const Ref& ref) override { auto f = places.find(ref.id); if (f == places.end() || !f->second.alive) return false; f->second.alive = false; return true; }
		void protect(const Ref& ref, bool on) override { if (on) protectedIds.insert(ref.id); else protectedIds.erase(ref.id); }
		std::map<std::string, double> flags;
		double playerX = 0.0;   // (where the player is along a street: 'past' lines)
		bool passed(const std::string& axis, double value, int way) const override { (void)axis; return way > 0 ? playerX >= value : playerX <= value; }
		std::vector<std::pair<uint64_t, int>> restarts;
		void restarted(uint64_t serial, int stage) override { restarts.push_back({ serial, stage }); }
		double flag(const std::string& name) const override { auto f = flags.find(name); return f != flags.end() ? f->second : 0.0; }
		void set_flag(const std::string& name, double value) override { flags[name] = value; }
		bool fact(const std::string& name, const Ref& ref) const override { return facts.count({ name, ref.id }) > 0; }
		bool set_fact(const std::string& name, const Ref& ref, bool on) override
		{
			if (on) facts.insert({ name, ref.id });
			else facts.erase({ name, ref.id });
			return true;
		}
		int alive_of(const std::string& type) const { int n = 0; for (const auto& [id, p] : places) n += p.type == type && p.alive && !p.culled ? 1 : 0; return n; }
	};

	const char* kSettings = R"({ "kinds": { "errand": { "priority": 50 }, "story": { "priority": 40 } },
		"signals": { "cache_opened": "binding", "enemy_down": "binding", "talked": "code", "alias_lost": "core", "player_down": "code", "horn": "core" },
		"flags": { "gate_open": "world.lua", "giver_friendly": "world.lua" },
		"limits": { "errand": 2 } })";

	const char* kRetrieve = R"({ "id": "retrieve", "kind": "errand", "repeatable": true, "title": "Q_RETRIEVE",
		"aliases": { "cache": { "find": "place", "type": "cache", "pick": "nearest", "where": { "meters": [70, 220] } } },
		"stages": [
			{ "id": 10, "objectives": [ "OBJ_RETRIEVE" ], "marker": "cache", "exits": [ { "on": { "signal": "cache_opened", "alias": "cache" }, "to": 100 } ] },
			{ "id": 100, "end": "complete", "reward": "retrieve_reward" } ],
		"fail": [ { "on": { "signal": "alias_lost", "alias": "cache" } } ] })";

	const char* kHunt = R"({ "id": "hunt", "kind": "errand", "repeatable": true, "title": "Q_HUNT", "limitSeconds": 600,
		"aliases": { "nest": { "find": "place", "type": "nest" } },
		"stages": [
			{ "id": 10, "objectives": [ "OBJ_HUNT" ], "marker": "nest", "checkpoint": true,
			  "exits": [ { "on": { "signal": "enemy_down", "count": 3 }, "to": 20 } ] },
			{ "id": 20, "objectives": [ "OBJ_HUNT_BACK" ], "exits": [ { "when": { "var": "back", "atLeast": 1 }, "to": 100 } ] },
			{ "id": 100, "end": "complete", "reward": "hunt_reward" } ] })";

	const char* kStory = R"({ "id": "the_pump", "kind": "story", "starts": { "by": "story" }, "onFail": "retry", "title": "Q_PUMP",
		"aliases": { "giver": { "find": "place", "type": "giver", "protect": true } },
		"stages": [
			{ "id": 10, "objectives": [ "OBJ_TALK" ], "marker": "giver", "exits": [ { "on": { "signal": "talked", "alias": "giver" }, "to": 20 } ] },
			{ "id": 20, "objectives": [ "OBJ_PUMP" ], "exits": [ { "on": { "signal": "cache_opened" }, "to": 100 } ] },
			{ "id": 100, "end": "complete", "reward": "pump_reward" } ],
		"fail": [ { "on": { "signal": "alias_lost", "alias": "giver" } } ] })";

	const char* kNest = R"({ "id": "nest_clear", "kind": "errand", "repeatable": true, "title": "Q_HUNT",
		"aliases": { "nest": { "find": "place", "type": "nest" } },
		"stages": [
			{ "id": 10, "objectives": [ "OBJ_HUNT" ], "marker": "nest",
			  "spawns": [ { "source": "brood", "type": "gator", "at": "nest", "count": [ 1, 3 ] } ],
			  "exits": [ { "cleared": true, "to": 100 } ] },
			{ "id": 100, "end": "complete" } ] })";

	// a story's chain for the fuzz: a guiding mark (repick, protected) that sets a flag and hands over (then) to
	// a step with an optional mark that ends on that flag
	const char* kChainA = R"({ "id": "s_mark", "kind": "story", "starts": { "by": "story" }, "onFail": "retry", "title": "Q_PUMP", "then": "s_next",
		"aliases": { "mark": { "find": "place", "type": "cache", "pick": "nearest", "repick": true, "protect": true } },
		"stages": [
			{ "id": 10, "objectives": [ "OBJ_TALK" ], "marker": "mark", "exits": [ { "on": { "signal": "talked" }, "to": 20 } ] },
			{ "id": 20, "objectives": [ "OBJ_PUMP" ], "set": { "gate_open": 1 }, "exits": [ { "when": { "flag": "gate_open", "atLeast": 1 }, "to": 100 } ] },
			{ "id": 100, "end": "complete" } ] })";
	const char* kChainB = R"({ "id": "s_next", "kind": "story", "starts": { "by": "story" }, "onFail": "retry", "title": "Q_PUMP",
		"aliases": { "nest": { "find": "place", "type": "nest", "pick": "nearest", "optional": true, "protect": true } },
		"stages": [ { "id": 10, "objectives": [ "OBJ_HUNT" ], "marker": "nest", "exits": [ { "when": { "flag": "gate_open", "atLeast": 1 }, "to": 100 } ] },
			{ "id": 100, "end": "complete" } ] })";

	QuestRecord parse(const char* text)
	{
		QuestRecord r;
		std::vector<std::string> errors;
		read_record(Json::parse(text), r, errors);
		for (const std::string& e : errors) std::printf("     %s\n", e.c_str());
		return r;
	}

	// The world of one test: a binding, an arbiter, the system, a ledger of the rewards granted (the game's)
	struct World
	{
		FakeBinding binding;
		Arbiter arbiter;
		QuestSystem quests;
		std::map<uint64_t, int> granted;   // serial -> times the game granted it (must stay 1)
		std::set<uint64_t> ledger;         // the game's own record of granted serials
		explicit World(uint32_t seed = 7) : quests(arbiter, binding, seed)
		{
			QuestSettings settings;
			std::vector<std::string> errors;
			read_settings(Json::parse(kSettings), settings, errors);
			quests.set_settings(settings);
			quests.add_record(parse(kRetrieve));
			quests.add_record(parse(kHunt));
			quests.add_record(parse(kStory));
			quests.add_record(parse(kNest));
			quests.add_record(parse(kChainA));
			quests.add_record(parse(kChainB));
			quests.set_reward_offer([this](uint64_t serial, const std::string&)
			{
				if (ledger.insert(serial).second) ++granted[serial];   // (the game grants a serial once)
			});
			binding.places[1] = { "cache", 150.0 };
			binding.places[2] = { "cache", 90.0 };
			binding.places[3] = { "cache", 500.0 };   // (out of the retrieve's meters)
			binding.places[4] = { "nest", 120.0 };
			binding.places[5] = { "giver", 30.0 };
		}
		uint64_t steps = 0;
		void step(double seconds = 0.1) { arbiter.begin_step(++steps); quests.step(steps, seconds); arbiter.resolve(steps); }
	};
}

int main(int argc, char** argv)
{
	// ---- MIS-V1..V4: the records and the checks
	{
		QuestSettings settings;
		std::vector<std::string> errors;
		check(read_settings(Json::parse(kSettings), settings, errors) && errors.empty(), "the settings read");
		QuestRecord r;
		check(read_record(Json::parse(kRetrieve), r, errors) && r.stages.size() == 2 && r.aliases.count("cache"), "a record reads");
		check(check_record(r, settings, {}).empty(), "a sound record checks clean");
		errors.clear();
		check(!read_record(Json::parse(R"({ "id": "x", "kind": "errand", "stagez": [] })"), r, errors), "an unknown field is refused (V1)");
		QuestRecord dead = parse(R"({ "id": "d", "kind": "errand", "stages": [ { "id": 10, "exits": [ { "on": { "signal": "talked" }, "to": 20 } ] },
			{ "id": 20, "exits": [ { "on": { "signal": "talked" }, "to": 10 } ] }, { "id": 30, "end": "complete" } ] })");
		const std::vector<std::string> deadFindings = check_record(dead, settings, {});
		bool loop = false, unreached = false;
		for (const std::string& f : deadFindings) { loop = loop || f.find("no way from it to an end") != std::string::npos; unreached = unreached || f.find("never reached") != std::string::npos; }
		check(loop && unreached, "a loop with no end and a stage never reached are found (V3)");
		QuestRecord unknownSignal = parse(R"({ "id": "u", "kind": "errand", "stages": [ { "id": 10, "exits": [ { "on": { "signal": "nobody_sends_it" }, "to": 100 } ] },
			{ "id": 100, "end": "complete" } ] })");
		check(!check_record(unknownSignal, settings, {}).empty(), "a signal outside the catalog is found (V4)");
		QuestRecord story = parse(R"({ "id": "s", "kind": "story", "starts": { "by": "story" }, "stages": [ { "id": 10, "end": "complete" } ] })");
		check(!check_record(story, settings, {}).empty(), "a story quest with no onFail is found");
		CheckContext texts;
		texts.textKnown = [](const std::string& t) { return t != "OBJ_RETRIEVE"; };
		check(!check_record(parse(kRetrieve), settings, texts).empty(), "an untranslated text is found (V2)");
	}

	// ---- MIS-T1: each transition
	{
		World w;
		const uint64_t s = w.quests.start("retrieve", "test");
		w.step();
		check(s && w.quests.quest(s)->aliases.at("cache").id == 2 && w.quests.quest(s)->stage == 10, "start: the nearest cache within its meters");
		check(w.arbiter.presentation().owner == "retrieve#" + std::to_string(s) && w.arbiter.presentation().marker->target.id == 2
			&& w.arbiter.presentation().objectives[0].text == "OBJ_RETRIEVE", "its stage claims its marker and its line");
		w.quests.signal("cache_opened", { "place", 1 });   // (another cache)
		w.step();
		check(w.quests.quest(s)->stage == 10, "a signal about another subject changes nothing");
		w.quests.signal("cache_opened", { "place", 2 });
		w.step();
		check(w.quests.quest(s)->state == "completed" && w.granted[s] == 1, "its cache opened: completed, its reward granted once");
		check(!w.arbiter.has_claim("retrieve#" + std::to_string(s)), "ended: its claim released");
		w.quests.signal("cache_opened", { "place", 2 });
		w.step();
		check(w.granted[s] == 1, "the same signal after the end changes nothing");
	}
	{
		World w;
		w.binding.places.erase(1);
		w.binding.places.erase(2);
		const size_t before = w.quests.log().size();
		check(w.quests.start("retrieve", "test") == 0 && w.quests.log().size() == before && w.arbiter.state()["claims"].empty(),
			"an alias that cannot be filled: no quest, no log, no claim");
	}
	{
		World w;
		const uint64_t a = w.quests.start("retrieve", "test");
		w.quests.abandon(a);   // (an errand: let go)
		w.step();
		check(w.quests.quest(a)->state == "abandoned" && !w.arbiter.has_claim("retrieve#" + std::to_string(a)), "abandon: abandoned, its claim released");
		const uint64_t b = w.quests.start("retrieve", "test");
		const uint64_t c = w.quests.start("retrieve", "test");
		check(b && c == 0, "a repeatable record runs once at a time");
		const uint64_t h = w.quests.start("hunt", "test");
		check(h != 0 && w.quests.start("hunt", "test") == 0, "the errand limit (2 at once) and one hunt at a time");
		for (uint64_t id : { 7ull, 8ull, 7ull, 9ull }) w.quests.signal("enemy_down", { "actor", id });
		w.step();
		check(w.quests.quest(h)->stage == 20, "a count of 3: three different subjects (one counted twice, once) (T2)");
		w.quests.set_var(h, "back", 1.0);
		w.step();
		check(w.quests.quest(h)->state == "completed", "a 'when' on a var: completed");
	}
	{
		World w;
		const uint64_t h = w.quests.start("hunt", "test");
		for (int i = 0; i < 6001; ++i) w.quests.step(++w.steps, 0.1);
		check(w.quests.quest(h)->state == "failed", "its limit (600 s): failed");
		const uint64_t r = w.quests.start("retrieve", "test");
		w.binding.places[2].alive = false;
		w.step();
		check(w.quests.quest(r)->state == "failed", "its cache gone (alias_lost): failed");
	}
	// ---- a story quest never stops: abandon refused, a fail retries from its start
	{
		World w;
		const uint64_t s = w.quests.start("the_pump", "story");
		w.quests.signal("talked", { "place", 5 });
		w.step();
		check(w.quests.quest(s)->stage == 20, "the story's first exit");
		w.quests.abandon(s);
		w.step();
		check(w.quests.quest(s)->state == "active", "a story quest's abandon is refused (and logged)");
		w.quests.fail(s, "a test");
		w.step();
		check(w.quests.quest(s)->state == "active" && w.quests.quest(s)->stage == 10, "a story quest's fail retries from its start");
		bool refused = false;
		for (const Json& e : w.quests.log()) refused = refused || e["outcome"] == "refused";
		check(refused, "the refusal is in the transition log");
	}

	// ---- a stage's spawns, owned by the quest; 'cleared' when every one is beaten; nothing left behind
	{
		const char* kClear = R"({ "id": "clear", "kind": "errand", "repeatable": true, "title": "Q_HUNT",
			"aliases": { "nest": { "find": "place", "type": "nest" } },
			"stages": [
				{ "id": 10, "objectives": [ "OBJ_HUNT" ], "marker": "nest",
				  "spawns": [ { "source": "guards", "type": "gator", "at": "nest", "count": [ 2, 4 ] } ],
				  "exits": [ { "cleared": true, "to": 100 } ] },
				{ "id": 100, "end": "complete" } ] })";
		const char* kOpener = R"({ "id": "opener", "kind": "errand", "title": "Q_RETRIEVE",
			"aliases": { "cache": { "find": "place", "type": "cache" } },
			"stages": [ { "id": 10, "objectives": [ "OBJ_RETRIEVE" ], "marker": "cache", "exits": [ { "fact": { "alias": "cache", "name": "opened" }, "to": 100 } ] },
				{ "id": 100, "end": "complete" } ] })";
		const char* kFetch = R"({ "id": "fetch", "kind": "errand", "title": "Q_RETRIEVE",
			"aliases": { "cache": { "find": "place", "type": "cache", "lost": "refill" } },
			"stages": [ { "id": 10, "objectives": [ "OBJ_RETRIEVE" ], "marker": "cache", "exits": [ { "on": { "signal": "cache_opened", "alias": "cache" }, "to": 100 } ] },
				{ "id": 100, "end": "complete" } ] })";
		{
			World w;
			w.quests.add_record(parse(kClear));
			const uint64_t s = w.quests.start("clear", "test");
			const Quest* q = w.quests.quest(s);
			const int made = static_cast<int>(q->vars.at("spawned.guards"));
			check(made >= 2 && made <= 4 && w.binding.alive_of("gator") == made && static_cast<int>(q->spawned.size()) == made,
				"a stage's spawns: 2 to 4 made on entry, each the quest's");
			std::vector<Ref> gators;
			for (const auto& [ref, source] : q->spawned) gators.push_back(ref);
			for (size_t i = 0; i + 1 < gators.size(); ++i) { w.quests.signal("enemy_down", gators[i]); w.binding.places[gators[i].id].alive = false; }   // (down: dead)
			w.quests.signal("enemy_down", gators[0]);   // (the same one twice: counted once)
			w.step();
			check(w.quests.quest(s)->state == "active" && w.quests.quest(s)->vars.at("beaten.guards") == made - 1, "one standing: not cleared (each beaten one counted once)");
			const uint64_t before = w.quests.revision();
			w.binding.places[gators[made - 2].id].alive = false;   // (killed again: already beaten, nothing new)
			w.step();
			const uint64_t same = w.quests.revision();
			World r;
			r.quests.add_record(parse(kClear));
			const uint64_t rs = r.quests.start("clear", "test");
			r.step();
			const uint64_t was = r.quests.revision();
			r.binding.places[r.quests.quest(rs)->spawned.begin()->first.id].alive = false;   // (one killed: no transition)
			r.step();
			check(same == before && r.quests.revision() > was && r.quests.quest(rs)->stage == 10, "a kill with no transition moves the revision (the game saves it)");
			w.binding.places[gators.back().id].alive = false;   // (the world took the last one, no signal)
			w.step();
			check(w.quests.quest(s)->state == "completed" && w.binding.alive_of("gator") == 0, "the last one gone: cleared, completed, nothing left");
			World k;
			k.quests.add_record(parse(kClear));
			const uint64_t c = k.quests.start("clear", "test");
			const Json all = k.quests.kill_target(c, "", "player");
			k.step();
			check(all["killed"].get<int>() >= 2 && k.quests.quest(c)->state == "completed", "quests.kill_target with no alias: every spawn downed, the stage cleared");
		}
		{
			World w;
			w.quests.add_record(parse(kClear));
			const uint64_t s = w.quests.start("clear", "test");
			const int made = static_cast<int>(w.quests.quest(s)->vars.at("spawned.guards"));
			w.quests.abandon(s);
			w.step();
			check(w.binding.alive_of("gator") == 0 && w.binding.despawned == made && w.quests.quest(s)->spawned.empty(), "abandoned: its spawns taken away (the teardown)");
		}
		{
			World a;
			a.quests.add_record(parse(kClear));
			const uint64_t s = a.quests.start("clear", "test");
			const int made = static_cast<int>(a.quests.quest(s)->vars.at("spawned.guards"));
			a.quests.signal("enemy_down", a.quests.quest(s)->spawned.begin()->first);
			a.step();
			const Json saved = a.quests.save();
			World b;
			b.quests.add_record(parse(kClear));
			std::vector<std::string> errors;
			b.quests.load(saved, errors);
			check(errors.empty() && b.binding.alive_of("gator") == made - 1 && static_cast<int>(b.quests.quest(s)->spawned.size()) == made - 1,
				"loaded: only the spawns still owed made again");
		}
		{
			World w;
			w.quests.add_record(parse(kOpener));
			w.binding.facts.insert({ "opened", 2 });   // (the player opened the nearest cache before the quest)
			const uint64_t s = w.quests.start("opener", "test");
			check(s && w.quests.quest(s)->state == "completed", "a fact already true: credited on entry (MIS-V7)");
			World v;
			v.quests.add_record(parse(kOpener));
			const uint64_t t = v.quests.start("opener", "test");
			v.step();
			check(v.quests.quest(t)->state == "active", "a fact not yet true: waits");
			v.binding.facts.insert({ "opened", v.quests.quest(t)->aliases.at("cache").id });
			v.step();
			check(v.quests.quest(t)->state == "completed", "the fact true later: the exit taken at the next step");
		}
		{
			World w;
			const uint64_t s = w.quests.start("the_pump", "test");
			check(w.binding.protectedIds.count(5) == 1, "a protected alias: protected while the quest holds it (MIS-V6)");
			w.quests.signal("talked", { "place", 5 });
			w.step();
			w.quests.signal("cache_opened", { "place", 1 });
			w.step();
			check(w.quests.quest(s)->state == "completed" && w.binding.protectedIds.empty(), "the quest ended: its target free to the world again");
			World k;
			const uint64_t p = k.quests.start("the_pump", "test");
			const Json refused = k.quests.kill_target(p, "giver", "world");
			check(refused["refused"] == true && refused["protected"] == true && k.binding.places[5].alive, "quests.kill_target by the world: a protected target refused");
			const Json done = k.quests.kill_target(p, "giver", "player");
			check(done["killed"] == 1 && !k.binding.places[5].alive, "by the player: downed (protection is against the world only)");
			const uint64_t g = k.quests.start("retrieve", "test");
			const Json moved = k.quests.teleport_to_alias(g, "cache", 4.0);
			check(moved["moved"] == true && k.binding.broughtTo.id == k.quests.quest(g)->aliases.at("cache").id && k.binding.broughtMeters == 4.0,
				"quests.teleport_to_alias: the player brought to the alias's target");
		}
		{
			World w;
			w.quests.add_record(parse(kFetch));
			const uint64_t s = w.quests.start("fetch", "test");
			const uint64_t first = w.quests.quest(s)->aliases.at("cache").id;
			w.binding.places[first].alive = false;
			w.step();
			const Ref now = w.quests.quest(s)->aliases.at("cache");
			bool refilled = false;
			for (const Json& e : w.quests.log()) refilled = refilled || e["outcome"] == "refill";
			check(w.quests.quest(s)->state == "active" && now.valid() && now.id != first && refilled, "lost: 'refill' takes another target, logged (no fail)");
		}
		{
			QuestSettings settings;
			std::vector<std::string> errors;
			read_settings(Json::parse(kSettings), settings, errors);
			QuestRecord story = parse(kStory);
			story.aliases["giver"].protect = false;
			check(!check_record(story, settings, {}).empty(), "a story quest's alias neither protected nor refilled is found (MIS-V6)");
			story.aliases["giver"].lost = "refill";
			bool v6 = false;
			for (const std::string& f : check_record(story, settings, {})) v6 = v6 || f.find("protected") != std::string::npos;
			check(!v6, "refilled instead: no V6 finding");
			Json noSpawns = Json::parse(kClear);
			noSpawns["stages"][0].erase("spawns");
			check(!check_record(parse(noSpawns.dump().c_str()), settings, {}).empty(), "'cleared' on a stage that spawns nothing is found");
		}
	}
	// ---- spawns and the world: a spawn streamed out is owed, not beaten (made again when the player is near); a
	// "release" teardown leaves the pack to the world; a protected alias the player must destroy is found; a broken
	// quest is logged
	{
		const char* kPack = R"({ "id": "pack", "kind": "errand", "title": "Q_HUNT",
			"aliases": { "nest": { "find": "place", "type": "nest" } },
			"stages": [ { "id": 10, "objectives": [ "OBJ_HUNT" ], "marker": "nest",
				"spawns": [ { "source": "p", "type": "gator", "at": "nest", "count": 3 } ], "exits": [ { "cleared": true, "to": 100 } ] },
				{ "id": 100, "end": "complete" } ] })";
		World w;
		QuestSettings settings = w.quests.settings();
		settings.spawnMeters = 60.0;
		w.quests.set_settings(settings);
		w.quests.add_record(parse(kPack));
		w.binding.places[4].distance = 30.0;   // (the player at the nest)
		const uint64_t s = w.quests.start("pack", "test");
		check(w.binding.alive_of("gator") == 3, "the player near: the pack made");
		for (auto& [id, p] : w.binding.places) if (p.type == "gator") p.culled = true;   // (the player walked away: streamed out)
		w.binding.places[4].distance = 500.0;
		for (int i = 0; i < 40; ++i) w.step();
		check(w.quests.quest(s)->state == "active" && w.quests.quest(s)->vars.at("beaten.p") == 0 && w.binding.alive_of("gator") == 0,
			"streamed out while the player is away: not beaten, not made again far from the player");
		w.binding.places[4].distance = 40.0;
		for (int i = 0; i < 31; ++i) w.step();
		check(w.binding.alive_of("gator") == 3 && w.quests.quest(s)->state == "active", "the player back: the pack owed made again");
		World cap;
		cap.quests.add_record(parse(kPack));
		cap.binding.refuseSpawns = 100;   // (the world full: nothing can be made)
		const uint64_t c = cap.quests.start("pack", "test");
		for (int i = 0; i < 40; ++i) cap.step();
		check(cap.quests.quest(c)->state == "active" && cap.quests.quest(c)->vars.at("beaten.p") == 0, "the world cannot make the pack: owed, never beaten (no errand won without its fight)");
		cap.binding.refuseSpawns = 0;
		for (int i = 0; i < 31; ++i) cap.step();
		check(cap.binding.alive_of("gator") == 3, "room again: the pack made at the next look");
		World r;
		Json released = Json::parse(kPack);
		released["stages"][0]["spawns"][0]["teardown"] = "release";
		r.quests.add_record(parse(released.dump().c_str()));
		const uint64_t t = r.quests.start("pack", "test");
		r.quests.abandon(t);
		r.step();
		check(r.binding.alive_of("gator") == 3 && r.binding.despawned == 0, "teardown 'release': the pack left to the world");
		World n;
		QuestSettings near = n.quests.settings();
		near.despawnMeters = 50.0;
		n.quests.set_settings(near);
		n.quests.add_record(parse(kPack));
		const uint64_t u = n.quests.start("pack", "test");
		n.quests.abandon(u);
		n.step();
		check(n.binding.alive_of("gator") == 3, "teardown near the player (20 m, under despawnMeters 50): not taken away in front of the player");
		{
			World m;
			m.quests.set_settings(near);
			m.quests.add_record(parse(kPack));
			const uint64_t v = m.quests.start("pack", "test");
			m.quests.abandon(v);
			m.step();
			std::vector<std::string> none;
			m.quests.load(Json::parse(R"({ "format": 1 })"), none);
			check(m.binding.alive_of("gator") == 0 && m.quests.step_times()["despawnWaiting"] == 0, "a load replacing the state: the removals waiting go at once");
		}
		for (auto& [id, p] : n.binding.places) if (p.type == "gator") p.distance = 80.0;
		for (int i = 0; i < 31; ++i) n.step();
		check(n.binding.alive_of("gator") == 0 && n.binding.despawned == 3, "the player far: taken away then (QA)");
		QuestSettings plain;
		std::vector<std::string> errors;
		read_settings(Json::parse(kSettings), plain, errors);
		Json guarded = Json::parse(kRetrieve);
		guarded["aliases"]["cache"]["protect"] = true;
		guarded["stages"][0]["exits"][0] = { { "fact", { { "alias", "cache" }, { "name", "destroyed" } } }, { "to", 100 } };
		bool found = false;
		for (const std::string& f : check_record(parse(guarded.dump().c_str()), plain, {})) found = found || f.find("must destroy") != std::string::npos;
		check(found, "a protected alias the player must destroy is found (QA)");
		World b;
		b.quests.load(Json::parse(R"({ "format": 1, "quests": { "2": { "record": "gone", "stage": 10 } } })"), errors);
		bool logged = false;
		for (const Json& e : b.quests.log()) logged = logged || e["outcome"] == "broken";
		check(logged && b.quests.quest(2)->state == "broken", "a broken quest is logged at the load (visible)");
	}
	// ---- while, timed events, flags, the stage's seconds, the journal
	{
		const char* kWatch = R"({ "id": "watch", "kind": "errand", "title": "Q_HUNT",
			"aliases": { "nest": { "find": "place", "type": "nest" } },
			"while": { "when": { "flag": "giver_friendly", "atLeast": 1 } },
			"stages": [
				{ "id": 10, "objectives": [ "OBJ_HUNT" ], "marker": "nest",
				  "events": [ { "at": 5, "signal": "horn" } ], "exits": [ { "on": { "signal": "horn" }, "to": 20 } ] },
				{ "id": 20, "objectives": [ "OBJ_HUNT" ], "events": [ { "at": 3, "spawn": { "source": "late", "type": "gator", "at": "nest", "count": 2 } } ],
				  "exits": [ { "cleared": true, "to": 30 } ] },
				{ "id": 30, "objectives": [ "OBJ_HUNT_BACK" ], "exits": [ { "when": { "flag": "gate_open", "atLeast": 1 }, "to": 40 } ] },
				{ "id": 40, "objectives": [ "OBJ_HUNT_BACK" ], "exits": [ { "when": { "var": "stage.elapsed", "atLeast": 30 }, "to": 100 } ] },
				{ "id": 100, "end": "complete" } ] })";
		World w;
		w.quests.add_record(parse(kWatch));
		w.binding.flags["giver_friendly"] = 1;
		const uint64_t s = w.quests.start("watch", "test");
		for (int i = 0; i < 4; ++i) w.step(1.0);
		check(w.quests.quest(s)->stage == 10, "a timed event not yet due: the stage waits");
		w.step(1.5);
		w.step();
		check(w.quests.quest(s)->stage == 20, "the event's signal at 5 s: the stage's exit taken");
		w.step(3.5);
		check(w.binding.alive_of("gator") == 2 && w.quests.quest(s)->stage == 20, "an event's spawn at 3 s: made, and the stage not cleared before it");
		const Json saved = w.quests.save();
		World b;
		b.quests.add_record(parse(kWatch));
		b.binding.flags["giver_friendly"] = 1;
		std::vector<std::string> errors;
		b.quests.load(saved, errors);
		check(b.binding.alive_of("gator") == 2, "a save after an event's spawn: loaded, the spawn owed is back (R14), the event not fired twice");
		for (auto& [id, p] : w.binding.places) if (p.type == "gator") p.alive = false;
		w.step();
		w.step();
		check(w.quests.quest(s)->stage == 30, "the event's spawns down: cleared");
		w.binding.flags["gate_open"] = 1;
		w.step();
		check(w.quests.quest(s)->stage == 40, "a 'when' on a flag of the game's store");
		w.step(20.0);
		check(w.quests.quest(s)->stage == 40, "a 'when' on the stage's seconds: not yet at 20 s");
		w.step(11.0);
		check(w.quests.quest(s)->state == "completed", "... taken at 31 s");
		World f;
		f.quests.add_record(parse(kWatch));
		f.binding.flags["giver_friendly"] = 1;
		const uint64_t t = f.quests.start("watch", "test");
		f.step();
		f.binding.flags["giver_friendly"] = 0;
		f.step();
		check(f.quests.quest(t)->state == "failed", "the quest's 'while' stopped holding: failed");
		const Json journal = f.quests.journal();
		check(journal.size() == 1 && journal[0]["state"] == "failed" && journal[0]["title"] == "Q_HUNT", "the journal lists it, with its title and state");
		QuestSettings plain;
		read_settings(Json::parse(kSettings), plain, errors);
		Json unknown = Json::parse(kWatch);
		unknown["stages"][2]["exits"][0]["when"]["flag"] = "nobody_owns_this";
		bool v4 = false;
		for (const std::string& line : check_record(parse(unknown.dump().c_str()), plain, {})) v4 = v4 || line.find("nobody_owns_this") != std::string::npos;
		check(v4, "a flag outside the catalog is found (MIS-V4)");
		// a story quest failed by its while: one retry, then it waits for the condition (no loop)
		World g;
		Json story = Json::parse(kStory);
		story["while"] = { { "when", { { "flag", "giver_friendly" }, { "atLeast", 1 } } } };
		g.quests.add_record(parse(story.dump().c_str()));
		g.binding.flags["giver_friendly"] = 1;
		const uint64_t u = g.quests.start("the_pump", "test");
		g.step();
		g.binding.flags["giver_friendly"] = 0;
		for (int i = 0; i < 50; ++i) g.step();
		int retries = 0;
		for (const Json& e : g.quests.log()) retries += e["outcome"] == "retry" ? 1 : 0;
		check(retries == 1 && g.quests.quest(u)->state == "active", "a story quest's while stopped: one retry, then it waits (no loop)");
	}
	// ---- a story's chain. 'reach' an alias, a stage's 'set' (flags), 'then' (the next record), a spawn's 'near'
	{
		const char* kVillage = R"({ "id": "the_village", "kind": "story", "starts": { "by": "story" }, "onFail": "retry", "title": "Q_PUMP",
			"then": "the_part",
			"aliases": { "village": { "find": "place", "type": "giver", "protect": true } },
			"stages": [ { "id": 10, "objectives": [ "OBJ_TALK" ], "marker": "village", "exits": [ { "reach": { "alias": "village", "meters": 12 }, "to": 100 } ] },
				{ "id": 100, "end": "complete", "set": { "gate_open": 1 } } ] })";
		const char* kPart = R"({ "id": "the_part", "kind": "story", "starts": { "by": "story" }, "onFail": "retry", "title": "Q_PUMP",
			"aliases": { "workshop": { "find": "place", "type": "nest", "protect": true } },
			"stages": [ { "id": 10, "objectives": [ "OBJ_HUNT" ], "marker": "workshop",
				"spawns": [ { "source": "held", "type": "gator", "at": "workshop", "count": 3, "near": 70 } ],
				"exits": [ { "cleared": true, "to": 100 } ] },
				{ "id": 100, "end": "complete" } ] })";
		World w;
		w.quests.add_record(parse(kVillage));
		w.quests.add_record(parse(kPart));
		w.binding.places[5].distance = 300.0;
		const uint64_t s = w.quests.start("the_village", "story");
		w.step();
		check(w.quests.quest(s)->state == "active", "reach: far, the stage waits");
		w.binding.places[5].distance = 10.0;
		w.step();
		check(w.quests.quest(s)->state == "completed" && w.binding.flags["gate_open"] == 1, "reach within 12 m: completed, its final stage's 'set' put the flag");
		{
			// a save between the end and the next step: the queued 'then' is in it, and starts after the load (#108 P1)
			const Json between = w.quests.save();
			World b;
			b.quests.add_record(parse(kVillage));
			b.quests.add_record(parse(kPart));
			std::vector<std::string> loadErrors;
			check(b.quests.load(between, loadErrors) && loadErrors.empty(), "a save between an end and its 'then' loads clean");
			b.step();
			bool started = false;
			for (const auto& [serial, q] : b.quests.quests()) started = started || (q.record == "the_part" && q.state == "active");
			check(started, "a 'then' queued when the save was made starts after the load (the chain does not stop)");
			check(b.quests.save().value("pending", Json::array()).empty(), "... and leaves the queue (no second start)");
		}
		w.step();   // (a then starts at the next step, never inside the end)
		uint64_t next = 0;
		for (const auto& [serial, q] : w.quests.quests()) if (q.record == "the_part") next = serial;
		check(next != 0 && w.quests.quest(next)->state == "active", "'then': the chain's next record started");
		w.binding.places[4].distance = 120.0;
		for (int i = 0; i < 31; ++i) w.step();
		check(w.binding.alive_of("gator") == 0, "a spawn's 'near' 70: not made at 120 m");
		w.binding.places[4].distance = 60.0;
		for (int i = 0; i < 31; ++i) w.step();
		check(w.binding.alive_of("gator") == 3, "... made at 60 m");
	}
	// ---- 'repick' (a guiding mark: the nearest open place of a kind, looked at again)
	{
		const char* kMark = R"({ "id": "mark", "kind": "story", "starts": { "by": "story" }, "onFail": "retry", "title": "Q_PUMP",
			"aliases": { "mark": { "find": "place", "type": "cache", "pick": "nearest", "repick": true, "protect": true } },
			"stages": [ { "id": 10, "objectives": [ "OBJ_RETRIEVE" ], "marker": "mark", "exits": [ { "on": { "signal": "talked" }, "to": 100 } ] },
				{ "id": 100, "end": "complete" } ] })";
		World w;
		w.quests.add_record(parse(kMark));
		const uint64_t s = w.quests.start("mark", "story");
		check(w.quests.quest(s)->aliases.at("mark").id == 2, "repick: the nearest cache at the start (90 m)");
		w.binding.places[1].distance = 80.0;   // (another a little nearer: not clearly)
		for (int i = 0; i < 31; ++i) w.step();
		check(w.quests.quest(s)->aliases.at("mark").id == 2, "another a little nearer (80 against 90): the mark stays (no flip)");
		w.binding.places[1].distance = 40.0;
		for (int i = 0; i < 31; ++i) w.step();
		check(w.quests.quest(s)->aliases.at("mark").id == 1 && w.arbiter.presentation().marker && w.arbiter.presentation().marker->target.id == 1
			&& w.binding.protectedIds.count(1) && !w.binding.protectedIds.count(2), "clearly nearer (40): the mark moves, the marker with it, the protection too");
		w.binding.places[1].alive = false;   // (that one no longer a candidate: opened)
		for (int i = 0; i < 31; ++i) w.step();
		check(w.quests.quest(s)->aliases.at("mark").id == 2, "its own no longer a candidate: the next nearest");
		const Json saved = w.quests.save();
		World b;
		b.quests.add_record(parse(kMark));
		std::vector<std::string> errors;
		b.quests.load(saved, errors);
		check(errors.empty() && b.quests.save() == saved, "a repicked alias saves and loads back");
		World none;
		none.quests.add_record(parse(kMark));
		for (auto& [id, place] : none.binding.places) if (place.type == "cache") place.alive = false;   // (none open)
		const uint64_t e = none.quests.start("mark", "story");
		check(e != 0 && !none.quests.quest(e)->aliases.at("mark").valid(), "no candidate at the start: the quest starts with its mark empty (never stuck)");
		none.binding.places[2].alive = true;
		for (int i = 0; i < 31; ++i) none.step();
		check(none.quests.quest(e)->aliases.at("mark").id == 2, "a candidate later: the mark taken");
	}
	// ---- review #108: a chain satisfied at once, even a cycle, never recurses (the then starts at the next step); the
	// validator finds the cycle and a then that names no record; two guiding marks never on one target
	{
		const char* kPing = R"({ "id": "ping", "kind": "errand", "repeatable": true, "title": "Q_HUNT", "then": "pong",
			"stages": [ { "id": 10, "objectives": [ "OBJ_HUNT" ], "exits": [ { "when": { "flag": "gate_open", "atLeast": 1 }, "to": 100 } ] },
				{ "id": 100, "end": "complete" } ] })";
		const char* kPong = R"({ "id": "pong", "kind": "errand", "repeatable": true, "title": "Q_HUNT", "then": "ping",
			"stages": [ { "id": 10, "objectives": [ "OBJ_HUNT" ], "exits": [ { "when": { "flag": "gate_open", "atLeast": 1 }, "to": 100 } ] },
				{ "id": 100, "end": "complete" } ] })";
		World w;
		QuestSettings open = w.quests.settings();
		open.limits.clear();
		w.quests.set_settings(open);
		w.quests.add_record(parse(kPing));
		w.quests.add_record(parse(kPong));
		w.binding.flags["gate_open"] = 1;   // (both satisfied at once: the cycle would run forever inside one call)
		const uint64_t s = w.quests.start("ping", "test");
		check(s && w.quests.quest(s)->state == "completed" && w.quests.quests().size() == 1, "a satisfied record's then is not started inside its end");
		for (int i = 0; i < 4; ++i) w.step();
		check(w.quests.quests().size() == 5, "a satisfied cycle runs one link a step (no recursion, no stack overflow)");
		std::map<std::string, QuestRecord> all = { { "ping", parse(kPing) }, { "pong", parse(kPong) } };
		bool cycle = false;
		for (const std::string& f : check_chains(all)) cycle = cycle || f.find("comes back") != std::string::npos;
		check(cycle, "the validator finds a then/onFail cycle");
		Json lone = Json::parse(kPing);
		lone["then"] = "nobody";
		bool missing = false;
		for (const std::string& f : check_chains({ { "ping", parse(lone.dump().c_str()) } })) missing = missing || f.find("no record") != std::string::npos;
		check(missing, "the validator finds a then that names no record");
		World two;
		const char* kMarkA = R"({ "id": "mark_a", "kind": "story", "starts": { "by": "story" }, "onFail": "retry", "title": "Q_PUMP",
			"aliases": { "mark": { "find": "place", "type": "cache", "pick": "nearest", "repick": true, "protect": true } },
			"stages": [ { "id": 10, "objectives": [ "OBJ_RETRIEVE" ], "marker": "mark", "exits": [ { "on": { "signal": "talked" }, "to": 100 } ] },
				{ "id": 100, "end": "complete" } ] })";
		Json b = Json::parse(kMarkA);
		b["id"] = "mark_b";
		two.quests.add_record(parse(kMarkA));
		two.quests.add_record(parse(b.dump().c_str()));
		const uint64_t qa = two.quests.start("mark_a", "test"), qb = two.quests.start("mark_b", "test");
		for (int i = 0; i < 61; ++i) two.step();
		const Ref ra = two.quests.quest(qa)->aliases.at("mark"), rb = two.quests.quest(qb)->aliases.at("mark");
		check(ra.valid() && rb.valid() && !(ra == rb), "two guiding marks near one target: never on the same one (reserved)");
	}
	// ---- a game of levels: legs back and forth along a street ('past' with its way), each leg's waves beaten (a flag the
	// genre raises) combined with it; a hold of 25 s at a line (away: reset); a level quest (never saved, its outcome
	// told, discarded with the level); guide() for a bot; the binding told of a checkpoint restart
	{
		const char* kLegs = R"({ "id": "tortillas", "kind": "errand", "scope": "level", "repeatable": true, "title": "Q_HUNT",
			"stages": [
				{ "id": 10, "objectives": [ "OBJ_HUNT" ], "exits": [ { "past": { "axis": "x", "value": 140, "way": 1 }, "when": { "flag": "waves.10", "atLeast": 1 }, "to": 20 } ] },
				{ "id": 20, "objectives": [ "OBJ_HUNT" ], "exits": [ { "past": { "axis": "x", "value": 40, "way": -1 }, "when": { "flag": "waves.20", "atLeast": 1 }, "to": 30 } ] },
				{ "id": 30, "objectives": [ "OBJ_HUNT" ], "checkpoint": true,
				  "exits": [ { "hold": { "past": { "axis": "x", "value": 40, "way": -1 }, "seconds": 25, "away": "reset" }, "when": { "flag": "waves.30", "atLeast": 1 }, "to": 100 } ] },
				{ "id": 100, "end": "complete" } ] })";
		World w;
		std::vector<std::string> outcomes;
		w.quests.set_outcome([&outcomes](const Quest& q, const std::string& outcome) { outcomes.push_back(q.record + ":" + outcome); });
		QuestSettings flagged = w.quests.settings();
		w.quests.set_settings(flagged);
		w.quests.add_record(parse(kLegs));
		const uint64_t s = w.quests.start("tortillas", "level");
		w.binding.playerX = 150.0;   // (past the first line, its waves not beaten yet)
		w.step();
		check(w.quests.quest(s)->stage == 10, "an exit's conditions combine: past the line, its waves not beaten: the leg waits");
		w.binding.flags["waves.10"] = 1.0;
		w.step();
		check(w.quests.quest(s)->stage == 20, "past the line AND its waves beaten: the next leg");
		const Json guide = w.quests.guide(s);
		check(guide["past"]["value"] == 40.0 && guide["past"]["way"] == -1, "guide(): the current leg's line and its way (back along the street), for the bot");
		w.binding.flags["waves.20"] = 1.0;
		w.step();
		check(w.quests.quest(s)->stage == 20, "the way is the leg's: at x 150 the line at 40 going back is not passed");
		w.binding.playerX = 30.0;
		w.step();
		check(w.quests.quest(s)->stage == 30, "back past 40: the third leg");
		w.binding.flags["waves.30"] = 1.0;
		for (int i = 0; i < 10; ++i) w.step(1.0);   // (10 s at the line)
		w.binding.playerX = 90.0;
		w.step(2.0);   // (away: the hold starts again)
		check(w.quests.quest(s)->stage == 30 && w.quests.guide(s)["hold"]["held"] == 0.0, "a hold left: away 'reset' starts it again");
		w.binding.playerX = 30.0;
		for (int i = 0; i < 24; ++i) w.step(1.0);
		check(w.quests.quest(s)->stage == 30, "24 s held of 25: still holding");
		w.quests.restart_checkpoint(s);
		w.step();
		check(!w.binding.restarts.empty() && w.binding.restarts.back() == std::make_pair(s, 30), "a checkpoint restart told to the binding (a genre's own waves resume)");
		w.step(1.0);
		check(w.quests.quest(s)->stage == 30 && w.quests.guide(s)["hold"]["held"] <= 1.5,
			"a checkpoint restart starts the hold from nothing (24 s before it and 1 s after are not 25: #110)");
		for (int i = 0; i < 26; ++i) w.step(1.0);
		check(w.quests.quest(s)->state == "completed" && !outcomes.empty() && outcomes.back() == "tortillas:completed", "25 s held, its waves beaten: completed, the outcome told to the game");
		check(w.quests.save()["quests"].empty(), "a level quest is never saved");
		const uint64_t t = w.quests.start("tortillas", "level");
		w.quests.end_level();
		check(!w.quests.quest(t) && outcomes.back() == "tortillas:unfinished" && !w.arbiter.has_claim("tortillas#" + std::to_string(t)),
			"the level ended: its quest discarded, told 'unfinished', its claim released");
		QuestSettings plain;
		std::vector<std::string> errors;
		read_settings(Json::parse(kSettings), plain, errors);
		Json bad = Json::parse(kLegs);
		bad["scope"] = "galaxy";
		errors.clear();
		QuestRecord r;
		read_record(bad, r, errors);
		check(!errors.empty(), "a scope that is not world or level is refused");
	}
	// ---- an exit's conditions on two targets: a signal of one alias AND a fact of another (each keeps its own: #110)
	{
		const char* kBell = R"({ "id": "bell", "kind": "errand", "title": "Q_HUNT",
			"aliases": { "bell": { "find": "place", "type": "cache", "pick": "nearest" }, "gate": { "find": "place", "type": "nest" } },
			"stages": [ { "id": 10, "exits": [ { "on": { "signal": "cache_opened", "alias": "bell" }, "fact": { "alias": "gate", "name": "ready" }, "to": 100 } ] },
				{ "id": 100, "end": "complete" } ] })";
		World w;
		w.quests.add_record(parse(kBell));
		const uint64_t s = w.quests.start("bell", "test");
		const Ref bell = w.quests.quest(s)->aliases.at("bell"), gate = w.quests.quest(s)->aliases.at("gate");
		check(bell.valid() && gate.valid() && !(bell == gate), "two aliases, two targets");
		w.binding.facts.insert({ "ready", gate.id });
		w.quests.signal("cache_opened", bell);
		w.step();
		check(w.quests.quest(s)->state == "completed", "the bell's signal with the gate ready: the exit holds (the fact read on the gate, not the bell)");
		World v;
		v.quests.add_record(parse(kBell));
		const uint64_t t = v.quests.start("bell", "test");
		const Ref bellV = v.quests.quest(t)->aliases.at("bell");
		v.binding.facts.insert({ "ready", bellV.id });   // (the fact on the wrong target)
		v.quests.signal("cache_opened", bellV);
		v.step();
		check(v.quests.quest(t)->state == "active", "the fact on the bell instead of the gate: the exit waits");
		QuestSettings plain;
		std::vector<std::string> errors;
		read_settings(Json::parse(kSettings), plain, errors);
		Json wrong = Json::parse(kBell);
		wrong["stages"][0]["exits"][0]["fact"]["alias"] = "nobody";
		bool found = false;
		for (const std::string& line : check_record(parse(wrong.dump().c_str()), plain, {})) found = found || line.find("'nobody'") != std::string::npos;
		check(found, "the validator checks every condition's alias (a fact's too)");
	}
	// ---- 'concurrent': one record running more than once, each on its own target (an outpost's capture per outpost)
	{
		const char* kCapture = R"({ "id": "capture", "kind": "story", "repeatable": true, "concurrent": true, "starts": { "by": "story" }, "onFail": "retry", "title": "Q_HUNT",
			"aliases": { "outpost": { "find": "place", "type": "cache", "pick": "named", "name": "given", "protect": true } },
			"stages": [ { "id": 10, "objectives": [ "OBJ_HUNT" ], "marker": "outpost", "exits": [ { "fact": { "alias": "outpost", "name": "held" }, "to": 100 } ] },
				{ "id": 100, "end": "complete" } ] })";
		World w;
		w.quests.add_record(parse(kCapture));
		const uint64_t a = w.quests.start("capture", "test", { { "outpost", Ref{ "place", 1 } } });
		const uint64_t b = w.quests.start("capture", "test", { { "outpost", Ref{ "place", 2 } } });
		const uint64_t c = w.quests.start("capture", "test", { { "outpost", Ref{ "place", 1 } } });
		check(a && b && !c, "concurrent: two at once on two targets, never two on one");
		w.binding.facts.insert({ "held", 2 });
		w.step();
		check(w.quests.quest(a)->state == "active" && w.quests.quest(b)->state == "completed", "each ends on its own target's fact");
	}
	// ---- checkpoints. A restart goes back to the last checkpoint entered, its spawns made anew; a story quest's
	// fail with onFail "checkpoint" too; the checkpoint survives a save
	{
		const char* kSiege = R"({ "id": "siege", "kind": "story", "starts": { "by": "story" }, "onFail": "checkpoint", "title": "Q_PUMP",
			"aliases": { "gate": { "find": "place", "type": "nest", "protect": true } },
			"stages": [
				{ "id": 10, "objectives": [ "OBJ_TALK" ], "marker": "gate", "exits": [ { "on": { "signal": "talked" }, "to": 20 } ] },
				{ "id": 20, "objectives": [ "OBJ_HUNT" ], "marker": "gate", "checkpoint": true,
				  "spawns": [ { "source": "wave", "type": "gator", "at": "gate", "count": 3 } ],
				  "exits": [ { "cleared": true, "to": 100 } ] },
				{ "id": 100, "end": "complete" } ],
			"fail": [ { "on": { "signal": "player_down" } } ] })";
		World w;
		w.quests.add_record(parse(kSiege));
		const uint64_t s = w.quests.start("siege", "test");
		w.quests.restart_checkpoint(s);
		w.step();
		check(w.quests.quest(s)->stage == 10, "a restart before any checkpoint: from the start");
		w.quests.signal("talked", { "place", 5 });
		w.step();
		const Quest* q = w.quests.quest(s);
		check(q->stage == 20 && q->checkpoint == 20 && w.binding.alive_of("gator") == 3, "the checkpoint stage entered: remembered, its wave made");
		w.binding.places[q->spawned.begin()->first.id].alive = false;   // (one down)
		w.step();
		w.quests.restart_checkpoint(s);
		w.step();
		check(w.quests.quest(s)->stage == 20 && w.quests.quest(s)->vars.at("beaten.wave") == 0 && w.binding.alive_of("gator") == 3,
			"restart: back at the checkpoint, the old wave taken away and a whole one made anew");
		w.quests.signal("player_down", { "actor", 1 });
		w.step();
		bool retried = false;
		for (const Json& e : w.quests.log()) retried = retried || (e["outcome"] == "retry" && e["to"] == 20);
		check(w.quests.quest(s)->state == "active" && w.quests.quest(s)->stage == 20 && retried && w.binding.alive_of("gator") == 3,
			"a story quest's fail with onFail checkpoint: from its checkpoint, not its start");
		const Json saved = w.quests.save();
		World b;
		b.quests.add_record(parse(kSiege));
		std::vector<std::string> errors;
		b.quests.load(saved, errors);
		check(errors.empty() && b.quests.quest(s)->checkpoint == 20 && b.quests.save() == saved, "the checkpoint saved and loaded back");
	}
	// ---- MIS-T3: a save mid-stage loads back the same, and plays on the same
	{
		World a;
		const uint64_t h = a.quests.start("hunt", "test");
		a.quests.signal("enemy_down", { "actor", 7 });
		a.step(12.5);
		const Json saved = a.quests.save();
		World b;
		std::vector<std::string> errors;
		check(b.quests.load(saved, errors) && errors.empty() && b.quests.save() == saved, "a save mid-stage loads back the same");
		check(b.arbiter.has_claim("hunt#" + std::to_string(h)), "loaded: its claim again");
		for (uint64_t id : { 7ull, 8ull, 9ull }) { a.quests.signal("enemy_down", { "actor", id }); b.quests.signal("enemy_down", { "actor", id }); }
		a.step(); b.step();
		check(a.quests.save() == b.quests.save() && b.quests.quest(h)->stage == 20, "the same signals after: the same state (7 counted once, before and after the load)");
		check(saved["quests"][std::to_string(h)]["elapsed"].get<double>() > 12.0, "elapsed seconds saved, never a step number");
	}
	// ---- the QUEST lines: kept at the step, written only by flush_log() (outside the step's timing)
	{
		World w;
		std::vector<std::string> lines;
		w.quests.set_log_line([&lines](const std::string& line) { lines.push_back(line); });
		w.quests.start("retrieve", "test");
		w.step();
		const size_t kept = w.quests.log().size();
		check(lines.empty() && kept > 0, "a step logs its transitions, and writes no line");
		w.quests.flush_log();
		check(lines.size() == kept && lines[0].rfind("QUEST {", 0) == 0, "flush_log: one QUEST line per transition");
		w.quests.flush_log();
		check(lines.size() == kept, "flushed again: nothing twice");
	}
	// ---- MIS-T3: a load between two random picks picks as the run that never saved (the draws are saved)
	{
		const char* kScout = R"({ "id": "scout", "kind": "errand", "repeatable": true, "title": "Q_RETRIEVE",
			"aliases": { "cache": { "find": "place", "type": "cache", "pick": "random" } },
			"stages": [
				{ "id": 10, "objectives": [ "OBJ_RETRIEVE" ], "marker": "cache", "exits": [ { "on": { "signal": "cache_opened", "alias": "cache" }, "to": 100 } ] },
				{ "id": 100, "end": "complete" } ] })";
		int same = 0, differs = 0;
		for (uint32_t seed = 1; seed <= 32; ++seed)
		{
			World a(seed), b(seed), c(seed);
			for (World* w : { &a, &b, &c }) w->quests.add_record(parse(kScout));
			const uint64_t first = a.quests.start("scout", "test");
			a.quests.signal("cache_opened", a.quests.quest(first)->aliases.at("cache"));
			a.step();
			Json saved = a.quests.save();
			std::vector<std::string> errors;
			b.quests.load(saved, errors);
			saved.erase("draws");   // (a save without the draws: the bug this guards against)
			c.quests.load(saved, errors);
			const Ref picked = a.quests.quest(a.quests.start("scout", "test"))->aliases.at("cache");
			if (b.quests.quest(b.quests.start("scout", "test"))->aliases.at("cache") == picked) ++same;
			if (!(c.quests.quest(c.quests.start("scout", "test"))->aliases.at("cache") == picked)) ++differs;
		}
		check(same == 32, "a load between two random picks: the second pick the same as without the load");
		check(differs > 0, "(and without the saved draws it would differ: the case can fail)");
	}
	// ---- a load that fails changes nothing (a newer build's save, a damaged block): the next save is the same
	{
		World w;
		const uint64_t s = w.quests.start("retrieve", "test");
		w.quests.signal("cache_opened", { "place", 2 });
		w.step();
		w.quests.start("hunt", "test");
		w.step();
		const Json before = w.quests.save();
		std::vector<std::string> errors;
		bool unchanged = true;
		for (const char* text : { R"({ "format": 2 })", R"({ "state": { "format": 2 } })", R"("quests")", R"([])", R"({ "format": "1" })",
			R"({ "format": 1, "quests": [] })", R"({ "format": 1, "done": 3 })", R"({ "format": 1, "nextSerial": -4 })" })
		{
			if (w.quests.load(Json::parse(text), errors)) unchanged = false;
			if (w.quests.save() != before) unchanged = false;
		}
		check(unchanged && w.quests.done_count("retrieve") == 1 && s != 0, "a load that fails leaves the state, and the next save, as they were");
	}
	// ---- a damaged quest in a save is left out, never a crash; the rest loads
	{
		World w;
		std::vector<std::string> errors;
		const bool loaded = w.quests.load(Json::parse(R"({ "format": 1, "nextSerial": 2, "done": {}, "rewarded": [],
			"quests": { "x": { "record": "retrieve", "stage": 10 }, "3": { "record": "hunt", "stage": 10, "aliases": { "nest": "place:4" } } } })"), errors);
		check(loaded && errors.size() == 1 && w.quests.quests().size() == 1 && w.quests.quest(3), "QA's save: the quest keyed 'x' left out (one line), the good one loaded");
	}
	// ---- a saved quest whose record or stage this build has not: kept as "broken", never stepped, saved back as it was
	{
		World w;
		std::vector<std::string> errors;
		const Json saved = Json::parse(R"({ "format": 1, "nextSerial": 9, "done": {}, "rewarded": [], "quests": {
			"4": { "record": "the_flood", "version": 3, "stage": 30, "aliases": { "dam": "place:77" }, "vars": { "x": 1 }, "counted": {}, "elapsed": 2, "total": 9 },
			"5": { "record": "the_pump", "version": 1, "stage": 70, "aliases": { "giver": "place:5" }, "vars": {}, "counted": {}, "elapsed": 0, "total": 0 } } })");
		const bool loaded = w.quests.load(saved, errors);
		for (int i = 0; i < 5; ++i) w.step(10.0);
		check(loaded && errors.size() == 2 && w.quests.quest(4)->state == "broken" && w.quests.quest(5)->state == "broken"
			&& !w.arbiter.has_claim(w.quests.quest(5)->owner()), "a record or a stage gone: kept as broken (a line each), not stepped, no claim");
		check(w.quests.save()["quests"] == saved["quests"], "saved back exactly as it was (the build that has them plays them on)");
	}
	// ---- a story quest whose giver is gone retries once, then waits for one (no fail on every step)
	{
		World w;
		const uint64_t s = w.quests.start("the_pump", "test");
		w.binding.places[5].alive = false;
		for (int i = 0; i < 50; ++i) w.step();
		int retries = 0;
		for (const Json& e : w.quests.log()) retries += e["outcome"] == "retry" ? 1 : 0;
		check(retries == 1 && w.quests.quest(s)->state == "active" && !w.quests.quest(s)->aliases.at("giver").valid(),
			"the giver gone: one retry, then the quest waits with no giver (50 steps, no loop)");
		w.binding.places[6] = { "giver", 40.0 };
		for (int i = 0; i < 31; ++i) w.step();   // (a waiting quest looks again every 30 steps)
		check(w.quests.quest(s)->aliases.at("giver").id == 6 && w.arbiter.presentation().marker && w.arbiter.presentation().marker->target.id == 6,
			"a giver again in the world: the quest takes it (within 30 steps), and its marker points at it");
		check(w.binding.protectedIds.count(6) == 1 && w.binding.protectedIds.count(5) == 0,
			"the new giver protected, the lost one no longer (MIS-V6 on a retry's refill)");
	}
	// ---- corrupt saves (keys dropped, wrong types, keys that are not serials, stages out of range, huge numbers):
	// never a crash; a refused one changes nothing; an accepted one plays on and saves back what it loaded
	{
		World base;
		const uint64_t a = base.quests.start("retrieve", "test");
		base.quests.start("hunt", "test");
		base.quests.start("the_pump", "test");
		base.quests.signal("enemy_down", { "actor", 1 });
		base.quests.signal("cache_opened", { "place", base.quests.quest(a)->aliases.at("cache").id });
		base.step();
		const Json good = base.quests.save();
		const Json values[] = { Json(), Json("x"), Json(-1), Json(0), Json(1e300), Json(-1e300), Json(18446744073709551615ull), Json(3.5),
			Json::array(), Json::object(), Json(true), Json(999), Json("place:999"), Json(INT64_MIN) };
		std::mt19937 random(424242u);
		int crashes = 0, refusedChanged = 0, acceptedBroken = 0, accepted = 0;
		for (int n = 0; n < 10000; ++n)
		{
			Json bad = good;
			const int mutations = 1 + static_cast<int>(random() % 3);
			for (int m = 0; m < mutations; ++m)
			{
				const Json flat = bad.flatten();
				if (flat.empty() || !bad.is_object()) break;
				auto pick = std::next(flat.begin(), static_cast<long>(random() % flat.size()));
				Json::json_pointer at(pick.key());
				for (int up = static_cast<int>(random() % 3); up > 0 && !at.empty(); --up) at = at.parent_pointer();
				if (at.empty()) at = Json::json_pointer("/quests");
				if (!bad.contains(at)) continue;
				switch (random() % 4)
				{
				case 0: { if (at.empty()) break; const std::string key = at.back(); Json& parent = bad.at(at.parent_pointer());
					if (parent.is_object()) parent.erase(key); } break;
				case 1: bad.at(at) = values[random() % std::size(values)]; break;
				case 2: if (bad.contains("quests") && bad["quests"].is_object() && !bad["quests"].empty())
					{
						const char* keys[] = { "x", "-1", "0", "99999999999999999999", "1e3", "", "007" };
						Json q = bad["quests"].begin().value();
						bad["quests"].erase(bad["quests"].begin().key());
						bad["quests"][keys[random() % std::size(keys)]] = q;
					} break;
				default: if (bad.contains("quests") && bad["quests"].is_object() && !bad["quests"].empty() && bad["quests"].begin().value().is_object())
					bad["quests"].begin().value()["stage"] = values[random() % std::size(values)]; break;
				}
			}
			World w;
			w.quests.start("retrieve", "before");
			w.step();
			const Json before = w.quests.save();
			std::vector<std::string> errors;
			try
			{
				if (!w.quests.load(bad, errors)) { if (w.quests.save() != before) ++refusedChanged; continue; }
				++accepted;
				const Json loaded = w.quests.save();
				World again;
				std::vector<std::string> errors2;
				if (!again.quests.load(loaded, errors2) || again.quests.save() != loaded) ++acceptedBroken;   // (a broken one is a line again)
				for (int i = 0; i < 3; ++i) w.step(5.0);
				for (const auto& [serial, q] : w.quests.quests())
				{
					const QuestRecord* r = w.quests.record(q.record);
					const Stage* st = r ? r->stage(q.stage) : nullptr;
					if (q.state == "active" && (!st || !st->end.empty() || !w.arbiter.has_claim(q.owner()))) ++acceptedBroken;
				}
			}
			catch (...) { ++crashes; }
		}
		std::printf("     corrupt saves: 10000 loads, %d accepted, %d refused\n", accepted, 10000 - accepted);
		check(crashes == 0, "10,000 corrupt saves: no exception, no crash");
		check(refusedChanged == 0, "a refused save never changes the state");
		check(acceptedBroken == 0, "an accepted save plays on (stages that exist, claims) and saves back what it loaded");
	}
	// ---- MIS-T5b: a kill between the two writes, either way round: the reward exactly once
	{
		// the quests' save ahead: completed and rewarded in it, the game's ledger without the serial
		World a;
		const uint64_t s = a.quests.start("retrieve", "test");
		a.quests.signal("cache_opened", { "place", 2 });
		a.step();
		const Json questsAhead = a.quests.save();
		World b;   // (the game restarted: its ledger lost the serial)
		std::vector<std::string> errors;
		b.quests.load(questsAhead, errors);
		check(b.granted[s] == 1, "the quests' save ahead: the reward granted at the load, once");
		World c;   // (a second load of the same: the ledger has it now)
		c.ledger = b.ledger;
		c.quests.load(questsAhead, errors);
		check(c.granted[s] == 0, "loaded again: not granted twice");
		// the game's save ahead: the ledger has the serial, the quests' save still active at the last stage
		World d;
		const uint64_t t = d.quests.start("retrieve", "test");
		d.step();
		const Json questsBehind = d.quests.save();
		World e;
		e.ledger.insert(t);   // (the game's save got the grant)
		e.quests.load(questsBehind, errors);
		e.quests.signal("cache_opened", { "place", 2 });   // (the completion happens again)
		e.step();
		check(e.granted[t] == 0 && e.quests.quest(t)->state == "completed", "the game's save ahead: the replayed completion skips the reward");
	}

	// ---- the tester's commands (MCP): none in a release; from another thread, carried out at the game's step; a call
	// in flight when the level goes gets an error
	{
		Pakal::McpServer server;
		World w;
		auto queue = std::make_shared<CommandQueue>();
		check(!add_command_tools(server, queue, "token", "1"), "a release: no command tools");
		check(add_command_tools(server, queue, "token", nullptr), "a development server with a token: the command tools");
		auto call = [&server](const std::string& tool, const std::string& args)
		{
			return server.handle_message("{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"" + tool
				+ "\",\"arguments\":" + args + "}}");
		};
		std::atomic<bool> done{ false };
		std::string started, signalled, saved, validated, bench, transitions, badAlias, noSerial;
		std::thread tester([&]()
		{
			started = call("quests.start", R"({"record":"retrieve"})");
			signalled = call("quests.signal", R"({"name":"cache_opened","kind":"place","id":2})");
			saved = call("quests.save", "{}");
			validated = call("quests.validate", "{}");
			bench = call("quests.bench", "{}");
			transitions = call("quests.transitions", R"({"since":0})");
			badAlias = call("quests.start", R"({"record":"retrieve","aliases":{"cache":"place:2"}})");
			noSerial = call("quests.kill_target", R"({"serial":null})");
			done = true;
		});
		for (int i = 0; i < 20000 && !done; ++i) { queue->drain(w.quests); w.step(); std::this_thread::sleep_for(std::chrono::microseconds(200)); }
		tester.join();
		check(started.find("serial") != std::string::npos && started.find("\"isError\":false") != std::string::npos, "quests.start from another thread: a serial");
		check(w.granted.size() == 1 && signalled.find("queued") != std::string::npos, "quests.signal: the retrieve completed at the game's step");
		check(badAlias.find("do not fit") != std::string::npos && noSerial.find("do not fit") != std::string::npos,
			"bad arguments: an answer saying they do not fit, never a raw exception");
		check(transitions.find("completed") != std::string::npos && transitions.find("retrieve") != std::string::npos, "quests.transitions: the system's own transitions");
		check(saved.find("nextSerial") != std::string::npos && validated.find("findings") != std::string::npos && bench.find("medianMs") != std::string::npos,
			"quests.save, quests.validate, quests.bench answer");
		queue->close();
		const std::string gone = call("quests.save", "{}");
		check(gone.find("\"isError\":true") != std::string::npos || gone.find("no level") != std::string::npos, "the level gone: a command gets an error, at once");
		remove_command_tools(server);
	}

	// ---- MIS-T8 (a synthetic load): 60 quests live at once (more than any game runs), stepped 3,000 times with
	// signals and transitions; the step's median, p95 and max printed (the budget, 0.5 ms a step, is measured on the
	// low-tier phone; here only a sanity bound)
	{
		Arbiter arbiter;
		FakeBinding binding;
		QuestSystem quests(arbiter, binding, 1u);
		QuestSettings settings;
		std::vector<std::string> errors;
		read_settings(Json::parse(kSettings), settings, errors);
		settings.limits.clear();
		quests.set_settings(settings);
		for (int i = 0; i < 60; ++i)
		{
			QuestRecord r = parse(kHunt);
			r.id = "hunt_" + std::to_string(i);
			r.aliases["nest"].type = "nest" + std::to_string(i);
			quests.add_record(r);
			binding.places[1000 + i] = { "nest" + std::to_string(i), 50.0 + i };
			quests.start(r.id, "bench");
		}
		// and a story quest waiting for a giver the world does not have (its refill looked for, throttled)
		quests.add_record(parse(kStory));
		binding.places[2000] = { "giver", 30.0 };
		quests.start("the_pump", "bench");
		binding.places[2000].alive = false;
		std::mt19937 random(5u);
		for (uint64_t s = 1; s <= 3000; ++s)
		{
			quests.signal("enemy_down", { "actor", random() % 40 });
			arbiter.begin_step(s);
			quests.step(s, 1.0 / 60.0);
			arbiter.resolve(s);
		}
		const Json times = quests.step_times();
		std::printf("     bench: %d quests live, median %.4f ms, p95 %.4f ms, max %.4f ms over %d steps\n", 60,
			times["medianMs"].get<double>(), times["p95Ms"].get<double>(), times["maxMs"].get<double>(), times["steps"].get<int>());
		check(times["p95Ms"].get<double>() < 0.5, "the bench: 60 quests live, p95 under 0.5 ms a step on this machine");
	}

	// ---- MIS-T5: seeded random sequences against the invariants
	{
		const uint32_t base = argc > 1 ? static_cast<uint32_t>(std::strtoul(argv[1], nullptr, 10)) : 20261123u;
		const int sequences = argc > 2 ? std::atoi(argv[2]) : 10000;
		int broken = 0;
		uint32_t firstBad = 0;
		std::string why;
		std::map<std::string, int> seen;   // every transition the fuzz made: "record from->to outcome"
		// V3 at run time: the stages of each record from which an end is reachable
		std::map<std::string, std::set<int>> canEnd;
		{
			World any;
			for (const auto& [id, record] : any.quests.records())
			{
				std::set<int>& ends = canEnd[id];
				for (const Stage& stage : record.stages) if (!stage.end.empty()) ends.insert(stage.id);
				for (bool grew = true; grew;)
				{
					grew = false;
					for (const Stage& stage : record.stages)
						for (const Exit& exit : stage.exits)
							if (ends.count(exit.to) && ends.insert(stage.id).second) grew = true;
				}
			}
		}
		for (int n = 0; n < sequences && broken == 0; ++n)
		{
			const uint32_t seed = base + static_cast<uint32_t>(n);
			std::mt19937 random(seed);
			World w(seed);
			const int events = 1 + static_cast<int>(random() % 200);
			std::set<uint64_t> retriedSinceInput;
			uint64_t seenSeq = 0;
			for (int e = 0; e < events && broken == 0; ++e)
			{
				const unsigned what = random() % 10;
				if (what != 9) retriedSinceInput.clear();   // (anything but time passing is an input)
				switch (what)
				{
				case 0: w.quests.request("errand", "fuzz"); break;
				case 1:
					if (random() % 2) w.quests.start("the_pump", "fuzz");
					else if (!w.quests.start("s_mark", "fuzz") && w.quests.last_refusal().find("alias") != std::string::npos)
					{
						broken = 1;   // (an optional mark never stops a start)
						why = "an optional mark stopped a story record's start: " + w.quests.last_refusal();
					}
					break;
				case 2: w.quests.signal("cache_opened", { "place", 1 + random() % 3 }); break;
				case 3:
				{
					// a spawn beaten (down and dead), or another actor down
					std::vector<uint64_t> standing;
					for (const auto& [id, place] : w.binding.places) if (id >= 100 && place.alive) standing.push_back(id);
					if (!standing.empty() && random() % 2)
					{
						const uint64_t id = standing[random() % standing.size()];
						w.binding.places[id].alive = false;
						if (random() % 4) w.quests.signal("enemy_down", { "place", id });   // (sometimes gone with no signal)
					}
					else w.quests.signal("enemy_down", { "actor", random() % 5 });
					break;
				}
				case 4: w.quests.signal("talked", { "place", 5 }); break;
				case 5: { const auto& q = w.quests.quests(); if (!q.empty()) w.quests.abandon(std::next(q.begin(), random() % q.size())->first); } break;
				case 6: { const auto& q = w.quests.quests(); if (!q.empty()) w.quests.set_var(std::next(q.begin(), random() % q.size())->first, "back", 1.0); } break;
				case 7: w.binding.places[1 + random() % 5].alive = random() % 4 != 0; break;
				case 8:
				{
					// a save and a load into a new world (the game restarted): the same state, and the rewards not twice
					const Json saved = w.quests.save();
					World again(seed);
					again.binding = w.binding;
					again.ledger = w.ledger;
					std::vector<std::string> errors;
					again.quests.load(saved, errors);
					if (again.quests.save() != saved) { broken = 1; why = "a save and load did not give the same state"; }
					for (const auto& [serial, times] : again.granted) if (times > 0) { broken = 1; why = "a load granted a reward already granted"; }
					break;
				}
				default: w.step(1.0 + random() % 30); break;
				}
				w.step();
				// the invariants: one state each; a claim while active and none after; no reward twice; no alias reserved by two
				std::set<Ref> held;
				for (const auto& [serial, q] : w.quests.quests())
				{
					const bool active = q.state == "active";
					if (q.state != "active" && q.state != "completed" && q.state != "failed" && q.state != "abandoned") { broken = 1; why = "a state outside the four"; }
					if (active != w.arbiter.has_claim(q.owner())) { broken = 1; why = "a claim not matching the state of " + q.owner(); }
					if (active) for (const auto& [name, ref] : q.aliases) if (ref.valid() && !held.insert(ref).second) { broken = 1; why = "one target held by two quests"; }
				}
				for (const auto& [serial, times] : w.granted) if (times > 1) { broken = 1; why = "a reward granted twice"; }
				for (const auto& [serial, q] : w.quests.quests())
					if (q.state == "active" && !canEnd[q.record].count(q.stage)) { broken = 1; why = "a live quest that can no longer end (V3)"; }
				// protection follows the marks: every protected place is held by a running quest that protects it (no orphan),
				// and every running quest's protected target is protected
				{
					std::set<uint64_t> wanted;
					for (const auto& [serial, q] : w.quests.quests())
					{
						if (q.state != "active") continue;
						const QuestRecord* r = w.quests.record(q.record);
						for (const auto& [name, ref] : q.aliases)
							if (r && r->aliases.count(name) && r->aliases.at(name).protect && ref.valid()) wanted.insert(ref.id);
					}
					if (wanted != w.binding.protectedIds) { broken = 1; why = "a protection not matching the running quests' protected marks (an orphan, or one lost)"; }
				}
				// a then-chain ends: its next record, started at the step after, ends on the flag its first set
				for (const auto& [serial, q] : w.quests.quests())
					if (q.record == "s_next" && q.state == "active" && w.binding.flags["gate_open"] >= 1 && q.elapsed > 1.0)
					{ broken = 1; why = "a then-chain's next record did not end on the flag set before it"; }
				// nothing left behind: every spawn standing belongs to a running quest
				for (const auto& [id, place] : w.binding.places)
				{
					if (id < 100 || !place.alive) continue;
					bool owned = false;
					for (const auto& [serial, q] : w.quests.quests()) owned = owned || (q.state == "active" && q.spawned.count({ "place", id }));
					if (!owned) { broken = 1; why = "a spawn left behind by a quest that ended"; }
				}
				for (const Json& entry : w.quests.log())
				{
					const uint64_t seq = entry["seq"].get<uint64_t>();
					if (seq <= seenSeq) continue;
					seenSeq = seq;
					if (entry["outcome"] != "retry") continue;
					if (!retriedSinceInput.insert(entry["serial"].get<uint64_t>()).second) { broken = 1; why = "a quest failed twice without an input"; }
				}
				if (w.quests.save().dump().size() > 64 * 1024) { broken = 1; why = "the save grows without bound"; }
			}
			for (const Json& e : w.quests.log())
				++seen[e["record"].get<std::string>() + " " + std::to_string(e["from"].get<int>()) + "->" + std::to_string(e["to"].get<int>()) + " "
					+ e["outcome"].get<std::string>()];
			if (broken) firstBad = seed;
		}
		// the coverage: every exit of every record, every start and end, reached at least once
		std::vector<std::string> wanted;
		World any;
		for (const auto& [id, record] : any.quests.records())
		{
			wanted.push_back(id + " 0->" + std::to_string(record.stages.front().id) + " start");
			for (const Stage& stage : record.stages)
				for (const Exit& exit : stage.exits) wanted.push_back(id + " " + std::to_string(stage.id) + "->" + std::to_string(exit.to) + " next");
		}
		int covered = 0;
		for (const std::string& edge : wanted)
		{
			if (seen.count(edge)) ++covered;
			else std::printf("     fuzz: never reached: %s\n", edge.c_str());
		}
		std::printf("     fuzz: %d of %zu transitions covered; outcomes:", covered, wanted.size());
		std::map<std::string, int> outcomes;
		for (const auto& [edge, n] : seen) outcomes[edge.substr(edge.rfind(' ') + 1)] += n;
		for (const auto& [outcome, n] : outcomes) std::printf(" %s %d", outcome.c_str(), n);
		std::printf("\n");
		check(covered == static_cast<int>(wanted.size()), "the fuzz reaches every transition of every record (T1 coverage)");
		if (broken) std::printf("     fuzz: seed %u broke it: %s (replay: QuestSystemTests %u 1)\n", firstBad, why.c_str(), firstBad);
		std::printf("     fuzz: seeds %u..%u, %d sequences\n", base, base + static_cast<uint32_t>(sequences) - 1, sequences);
		check(broken == 0, "the fuzz: one state per quest, claims match, no reward twice, no target held twice, saves round-trip, bounded, "
			"every live quest can end (V3), no two fails without an input, protection follows the marks, optional marks never stop a start, then-chains end");
	}
	// ---- what a stage waits on, read by the tools (the journal's 'waits', the quests' panel)
	{
		Exit signal;
		signal.signal = "cache_opened";
		signal.alias = "cache";
		signal.count = 3;
		check(describe_exit(signal) == "signal cache_opened x3 of cache", "an exit read: its signal, count and subject");
		Exit held;
		held.pastAxis = "x";
		held.pastValue = 140;
		held.pastWay = -1;
		held.holdSeconds = 20;
		held.holdAway = "reset";
		held.whenFlag = "waves";
		held.atLeast = 1;
		check(describe_exit(held) == "flag waves >= 1 and past x 140 going -1 and hold 20 s past the line (away: reset)", "an exit read: all its conditions, joined");
		check(describe_exit(Exit()) == "nothing (always)", "an exit with no condition read as one that always holds");
		World f;
		f.quests.add_record(parse(R"({ "id": "wait", "kind": "errand",
			"stages": [ { "id": 10, "exits": [ { "when": { "flag": "gate_open", "atLeast": 1 }, "to": 100 } ] }, { "id": 100, "end": "complete" } ] })"));
		f.quests.start("wait", "test");
		f.step();
		const Json journal = f.quests.journal();
		check(journal.size() == 1 && journal[0]["waits"] == Json::array({ "flag gate_open >= 1 -> 100" }), "the journal says what a running quest waits on");
		World g;
		g.quests.add_record(parse(R"({ "id": "held", "kind": "errand", "aliases": { "camp": { "find": "place", "type": "cache" } },
			"stages": [ { "id": 10, "exits": [ { "fact": { "alias": "camp", "name": "held" }, "to": 100 } ] }, { "id": 100, "end": "complete" } ] })"));
		const uint64_t h = g.quests.start("held", "test");
		g.step();
		const Ref camp = g.quests.quest(h)->aliases.at("camp");
		check(g.quests.set_fact(camp, "held", true)["set"] == true, "a tester's fact put through the binding");
		g.step();
		check(g.quests.quest(h)->state == "completed", "a stage that waits on the world's fact: played by a tester's fact");
		check(g.quests.set_fact(Ref{}, "held", true)["set"] == false, "a fact of no target: refused");
	}
	// a 'use' and the player's quest items: pick the part up at the workshop (the player there, the use made), carry it,
	// hand it over at the pump. Walking by credits nothing, a use from afar or without the item is refused, and the item
	// is saved with the quests
	{
		const char* kGasket = R"({ "id": "gasket", "kind": "story", "starts": { "by": "story" }, "onFail": "retry",
			"aliases": { "workshop": { "find": "place", "type": "workshop", "protect": true }, "pump": { "find": "place", "type": "pump", "protect": true } },
			"stages": [
				{ "id": 10, "marker": "workshop", "exits": [ { "use": { "alias": "workshop", "meters": 4, "action": "pickup" }, "gives": "gasket", "to": 20 } ] },
				{ "id": 20, "marker": "pump", "exits": [ { "use": { "alias": "pump", "meters": 4, "action": "install" }, "needs": "gasket", "to": 100 } ] },
				{ "id": 100, "end": "complete" } ] })";
		World u;
		u.quests.add_record(parse(kGasket));
		u.binding.places[50] = { "workshop", 50.0 };
		u.binding.places[51] = { "pump", 50.0 };
		const uint64_t s = u.quests.start("gasket", "test");
		u.step();
		check(s && u.quests.offers().empty(), "use: nothing offered while the player is far");
		u.quests.use(s, 10, 0);
		u.step();
		check(u.quests.quest(s)->stage == 10 && u.quests.log().back()["outcome"] == "refused", "use: one made from afar is refused (logged)");
		u.binding.places[50].distance = 2.0;
		u.step();
		u.step();
		check(u.quests.quest(s)->stage == 10, "use: standing at the workshop credits nothing by itself");
		const auto offers = u.quests.offers();
		check(offers.size() == 1 && offers[0].action == "pickup" && offers[0].alias == "workshop" && offers[0].exit == 0, "use: offered when near, with its action");
		u.quests.use(s, 10, 0);
		u.step();
		check(u.quests.quest(s)->stage == 20 && u.quests.items().count("gasket") && u.quests.items().at("gasket") == 1, "use: made, the stage left and the part given");
		u.quests.use(s, 10, 0);
		u.step();
		check(u.quests.quest(s)->stage == 20 && u.quests.items().at("gasket") == 1, "use: one of a stage already left is refused, nothing given twice");
		const Json journal = u.quests.journal();
		check(!journal.empty() && journal[0]["waits"] == Json::array({ "use (install) pump within 4 m and carrying gasket (handed over) -> 100" }), "use: the journal says what it waits on");
		const Json guide = u.quests.guide(s);
		check(guide.contains("use") && guide["use"]["alias"] == "pump" && guide["use"]["action"] == "install", "use: the guide gives a bot the use to make");
		// saved with the part carried, loaded in another world
		const Json saved = u.quests.save();
		check(saved.contains("items") && saved["items"]["gasket"] == 1, "items: saved with the quests");
		World v;
		v.quests.add_record(parse(kGasket));
		v.binding.places[50] = { "workshop", 2.0 };
		v.binding.places[51] = { "pump", 2.0 };
		std::vector<std::string> errors;
		check(v.quests.load(saved, errors) && errors.empty() && v.quests.items().at("gasket") == 1, "items: loaded back");
		v.quests.set_item("gasket", 0);
		v.step();
		check(v.quests.offers().empty(), "needs: not offered without the item");
		v.quests.use(s, 20, 0);
		v.step();
		check(v.quests.quest(s)->stage == 20, "needs: a use without the item is refused");
		v.quests.set_item("gasket", 1);
		v.step();
		check(v.quests.offers().size() == 1 && v.quests.offers()[0].action == "install", "needs: offered again with the item");
		v.quests.use(s, 20, 0);
		v.step();
		check(v.quests.quest(s)->state == "completed" && v.quests.items().empty(), "needs: the use made, the part handed over, the quest done");
		bool logged = false;
		for (const Json& e : v.quests.log()) logged = logged || (e["outcome"] == "items" && e["handed"] == Json::array({ "gasket" }));
		check(logged, "items: what was handed over is in the transitions' log");
		// the validator: the item in the catalog, an item needed that no record gives, a use without an alias
		QuestSettings settings = u.quests.settings();
		const QuestRecord record = parse(kGasket);
		bool missing = false;
		for (const std::string& f : check_record(record, settings, {})) missing = missing || f.find("'gasket' is not in the catalog") != std::string::npos;
		check(missing, "check: an item not in the catalog is a finding");
		settings.items["gasket"] = "ITEM_GASKET";
		bool still = false;
		for (const std::string& f : check_record(record, settings, {})) still = still || f.find("gasket") != std::string::npos;
		check(!still, "check: an item in the catalog is sound");
		std::map<std::string, QuestRecord> records = { { "gasket", record } };
		check(check_chains(records).empty(), "check: an item needed and given is sound");
		records["gasket"].stages[0].exits[0].gives.clear();
		bool ungiven = false;
		for (const std::string& f : check_chains(records)) ungiven = ungiven || f.find("needs the item 'gasket', which no record gives") != std::string::npos;
		check(ungiven, "check: an item needed that no record gives is a finding");
		QuestRecord bad;
		std::vector<std::string> readErrors;
		read_record(Json::parse(R"({ "id": "bad", "kind": "errand", "stages": [ { "id": 10, "exits": [ { "use": { "meters": 3 }, "to": 100 } ] }, { "id": 100, "end": "complete" } ] })"), bad, readErrors);
		check(!readErrors.empty(), "read: a use with no alias is refused");
	}
	// a stage's 'starts': the records it starts beside it when it is entered (the workshop's occupiers, cleared on their
	// own while the player sneaks the part out), at the next step, once, saved while pending; checked by check_chains
	{
		const char* kPart = R"({ "id": "part", "kind": "story", "starts": { "by": "story" }, "onFail": "retry",
			"aliases": { "shop": { "find": "place", "type": "giver", "protect": true } },
			"stages": [
				{ "id": 10, "starts": "occupiers", "exits": [ { "when": { "var": "taken", "atLeast": 1 }, "to": 100 } ] },
				{ "id": 100, "end": "complete" } ] })";
		const char* kOccupiers = R"({ "id": "occupiers", "kind": "story", "starts": { "by": "story" }, "onFail": "retry",
			"aliases": { "shop": { "find": "place", "type": "giver", "reserve": false, "protect": true } },
			"stages": [ { "id": 10, "exits": [ { "when": { "var": "down", "atLeast": 1 }, "to": 100 } ] }, { "id": 100, "end": "complete" } ] })";
		World s;
		s.quests.add_record(parse(kPart));
		s.quests.add_record(parse(kOccupiers));
		const uint64_t part = s.quests.start("part", "test");
		check(part && !s.quests.running("occupiers"), "starts: nothing started inside the transition (at the next step)");
		const Json pendingSave = s.quests.save();
		bool savedStart = false;
		for (const Json& p : pendingSave.value("pending", Json::array())) savedStart = savedStart || p.value("start", std::string()) == "occupiers";
		check(savedStart, "starts: a start still pending is saved");
		s.step();
		if (!s.quests.running("occupiers")) std::printf("     refused: %s\n", s.quests.last_refusal().c_str());
		check(s.quests.running("occupiers"), "starts: the record started beside the quest at the next step");
		bool logged = false;
		for (const Json& e : s.quests.log()) logged = logged || (e["record"] == "occupiers" && e["cause"] == "starts of part");
		check(logged, "starts: the start says which quest started it");
		// the occupiers share the shop with the part's quest because their alias does not reserve; one that reserves is
		// still kept off a target another quest reserves
		s.quests.add_record(parse(R"({ "id": "rival", "kind": "errand", "aliases": { "shop": { "find": "place", "type": "giver" } },
			"stages": [ { "id": 10, "exits": [ { "when": { "var": "x", "atLeast": 1 }, "to": 100 } ] }, { "id": 100, "end": "complete" } ] })"));
		check(s.quests.start("rival", "test") == 0, "reserve: a reserving alias is kept off a target another quest reserves");
		s.quests.set_var(part, "taken", 1);
		s.step();
		check(s.quests.quest(part) && s.quests.quest(part)->state == "completed" && s.quests.running("occupiers"), "starts: it lives on its own after the quest that started it ends");
		uint64_t occupiers = 0;
		for (const auto& [serial, q] : s.quests.quests()) if (q.record == "occupiers") occupiers = serial;
		s.quests.start("part", "test again");   // (a story record done: not started again, so neither is what it starts)
		s.step();
		int count = 0;
		for (const auto& [serial, q] : s.quests.quests()) count += q.record == "occupiers" ? 1 : 0;
		check(count == 1, "starts: one already running is not started twice");
		s.quests.set_var(occupiers, "down", 1);
		s.step();
		check(s.quests.quest(occupiers) && s.quests.quest(occupiers)->state == "completed", "starts: the started record ends by its own exits");
		std::map<std::string, QuestRecord> records = { { "part", parse(kPart) } };
		bool missing = false;
		for (const std::string& f : check_chains(records)) missing = missing || f.find("starts 'occupiers', which is no record") != std::string::npos;
		check(missing, "check: a stage starting a record that does not exist is a finding");
		records["occupiers"] = parse(kOccupiers);
		check(check_chains(records).empty(), "check: a stage starting an existing record is sound");
	}
	// a stage's choice: its 'choose' exits asked together (the prompt, who asks, the options in order), the option picked
	// leading to its own branch; one not offered (another stage, its condition not holding) refused; the pick kept in the
	// save and never asked again; the validator's findings
	{
		const char* kDeliver = R"({ "id": "deliver", "kind": "story", "starts": { "by": "story" }, "onFail": "retry",
			"aliases": { "mateo": { "find": "place", "type": "giver", "protect": true } },
			"stages": [
				{ "id": 10, "exits": [ { "when": { "var": "talked", "atLeast": 1 }, "to": 20 } ] },
				{ "id": 20, "choice": { "prompt": "Q_DELIVER", "speaker": "mateo" }, "exits": [
					{ "choose": { "option": "mateo", "text": "OPT_MATEO" }, "to": 30 },
					{ "choose": { "option": "luz", "text": "OPT_LUZ" }, "to": 40 },
					{ "choose": { "option": "split", "text": "OPT_SPLIT" }, "when": { "flag": "gate_open", "atLeast": 1 }, "to": 50 } ] },
				{ "id": 30, "set": { "giver_friendly": 1 }, "exits": [ { "when": { "var": "x", "atLeast": 1 }, "to": 100 } ] },
				{ "id": 40, "exits": [ { "when": { "var": "x", "atLeast": 1 }, "to": 100 } ] },
				{ "id": 50, "exits": [ { "when": { "var": "x", "atLeast": 1 }, "to": 100 } ] },
				{ "id": 100, "end": "complete" } ] })";
		World c;
		c.quests.add_record(parse(kDeliver));
		const uint64_t d = c.quests.start("deliver", "test");
		c.step();
		check(d && c.quests.choices().empty(), "choice: nothing asked before its stage");
		c.quests.choose(d, 20, "mateo");
		c.step();
		check(c.quests.quest(d)->stage == 10 && c.quests.log().back()["outcome"] == "refused", "choice: a pick before it is asked is refused (logged)");
		c.quests.set_var(d, "talked", 1);
		c.step();
		auto asked = c.quests.choices();
		check(asked.size() == 1 && asked[0].prompt == "Q_DELIVER" && asked[0].speakerAlias == "mateo" && asked[0].speaker.valid(), "choice: asked at its stage, its prompt and who asks");
		check(asked.size() == 1 && asked[0].options.size() == 2 && asked[0].options[0].id == "mateo" && asked[0].options[1].text == "OPT_LUZ",
			"choice: the options in the record's order; one whose condition does not hold is not offered");
		c.quests.choose(d, 20, "split");
		c.step();
		check(c.quests.quest(d)->stage == 20 && c.quests.chosen("deliver", 20).empty(), "choice: an option not offered is refused, nothing kept");
		c.quests.choose(d, 20, "mateo");
		c.step();
		check(c.quests.quest(d)->stage == 30 && c.quests.chosen("deliver", 20) == "mateo" && c.binding.flags["giver_friendly"] == 1.0,
			"choice: the option picked, its branch taken (its flag put), the pick kept");
		check(c.quests.choices().empty(), "choice: answered, never asked again");
		bool logged = false;
		for (const Json& e : c.quests.log()) logged = logged || (e["outcome"] == "choice" && e["cause"] == "the player chose 'mateo'");
		check(logged, "choice: the pick in the transitions' log");
		const Json saved = c.quests.save();
		check(saved.contains("chosen") && saved["chosen"]["deliver:20"] == "mateo", "choice: the pick saved");
		World back;
		back.quests.add_record(parse(kDeliver));
		std::vector<std::string> errors;
		check(back.quests.load(saved, errors) && back.quests.chosen("deliver", 20) == "mateo" && back.quests.quest(d)->stage == 30,
			"choice: loaded, the pick and its branch are the same (a reload never changes it)");
		// the validator
		QuestRecord one = parse(kDeliver);
		one.stages[1].exits.resize(1);
		bool single = false;
		for (const std::string& f : check_record(one, c.quests.settings(), {})) single = single || f.find("a choice of one option") != std::string::npos;
		check(single, "check: a choice of one option is a finding");
		QuestRecord twice = parse(kDeliver);
		twice.stages[1].exits[1].chooseOption = "mateo";
		bool dup = false;
		for (const std::string& f : check_record(twice, c.quests.settings(), {})) dup = dup || f.find("the option 'mateo' twice") != std::string::npos;
		check(dup, "check: an option twice in a choice is a finding");
		QuestRecord unasked = parse(kDeliver);
		unasked.stages[1].choicePrompt.clear();
		bool noPrompt = false;
		for (const std::string& f : check_record(unasked, c.quests.settings(), {})) noPrompt = noPrompt || f.find("no 'choice' asking them") != std::string::npos;
		check(noPrompt, "check: options with no prompt are a finding");
		bool clean = true;
		for (const std::string& f : check_record(parse(kDeliver), c.quests.settings(), {})) clean = clean && f.find("choice") == std::string::npos && f.find("option") == std::string::npos;
		check(clean, "check: a sound choice has no finding");
		QuestRecord bad;
		std::vector<std::string> readErrors;
		read_record(Json::parse(R"({ "id": "bad", "kind": "errand", "stages": [ { "id": 10, "choice": { "prompt": "Q" }, "exits": [
			{ "choose": { "option": "a" }, "to": 100 }, { "choose": { "option": "b", "text": "B" }, "on": { "signal": "talked" }, "to": 100 } ] },
			{ "id": 100, "end": "complete" } ] })"), bad, readErrors);
		check(readErrors.size() >= 2, "read: an option with no text, and one joined with a signal, are refused");
	}
	// a choice answered, then its stage entered again (the quest back at an earlier checkpoint): the same branch taken on
	// entry, the choice never asked again, the other option refused, the other branch's flag never put
	{
		const char* kAgain = R"({ "id": "again", "kind": "story", "starts": { "by": "story" }, "onFail": "checkpoint",
			"aliases": { "mateo": { "find": "place", "type": "giver", "protect": true } },
			"stages": [
				{ "id": 10, "checkpoint": true, "exits": [ { "when": { "var": "talked", "atLeast": 1 }, "to": 20 } ] },
				{ "id": 20, "choice": { "prompt": "Q_DELIVER", "speaker": "mateo" }, "exits": [
					{ "choose": { "option": "mateo", "text": "OPT_MATEO" }, "to": 30 },
					{ "choose": { "option": "luz", "text": "OPT_LUZ" }, "to": 40 } ] },
				{ "id": 30, "set": { "giver_friendly": 1 }, "exits": [ { "when": { "var": "x", "atLeast": 1 }, "to": 100 } ] },
				{ "id": 40, "set": { "gate_open": 1 }, "exits": [ { "when": { "var": "x", "atLeast": 1 }, "to": 100 } ] },
				{ "id": 100, "end": "complete" } ] })";
		World a;
		a.quests.add_record(parse(kAgain));
		const uint64_t q = a.quests.start("again", "test");
		a.quests.set_var(q, "talked", 1);
		a.step();
		a.quests.choose(q, 20, "mateo");
		a.step();
		check(a.quests.quest(q)->stage == 30 && a.binding.flags["giver_friendly"] == 1.0, "choice again: answered, its branch taken");
		a.binding.flags["giver_friendly"] = 0.0;   // (to see whether the branch puts it again)
		a.quests.restart_checkpoint(q);            // (back to stage 10; 'talked' kept: stage 20 is entered again at once)
		a.step();
		check(a.quests.choices().empty(), "choice again: its stage entered again, the choice is not asked");
		check(a.quests.quest(q)->stage == 30 && a.binding.flags["giver_friendly"] == 1.0, "choice again: the same branch taken on entry");
		a.quests.force_stage(q, 20);   // (a tester's set_stage to the choice itself)
		a.quests.choose(q, 20, "luz");
		a.step();
		a.step();
		check(a.quests.quest(q)->stage == 30 && a.quests.chosen("again", 20) == "mateo" && a.binding.flags["gate_open"] == 0.0,
			"choice again: the other option refused, the pick kept, the other branch's flag never put");
		// an option the record no longer has: forgotten and asked again
		std::vector<std::string> errors;
		Json saved = a.quests.save();
		saved["chosen"]["again:20"] = "gone";
		World b;
		b.quests.add_record(parse(kAgain));
		b.quests.load(saved, errors);
		b.quests.force_stage(q, 20);
		b.step();
		check(b.quests.choices().size() == 1 && b.quests.chosen("again", 20).empty(), "choice again: an option no longer in the record is forgotten, the choice asked again");
	}
	std::printf("%s\n", failures ? "SOME CHECKS FAILED" : "all checks passed");
	return failures ? 1 : 0;
}
