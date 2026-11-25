///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: QuestTemplatesTests.cpp
// Purpose: The designers' quest templates (game_genres/quests/templates, docs/QUEST_GUIDE.md) stay sound:
//          each record reads and checks clean against the templates' settings, the chains find their
//          records, and each file plays to its end on a made-up world, a stand-in player doing what each
//          stage's first exit asks (a use, a choice, its guards down, a hold, a reach, a flag).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "quests/QuestRecord.h"
#include "quests/QuestSystem.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace Pakal::Quests;
using Json = nlohmann::json;

namespace
{
	int failures = 0;
	void check(bool condition, const std::string& what)
	{
		std::printf("%s %s\n", condition ? "ok  " : "FAIL", what.c_str());
		std::fflush(stdout);
		if (!condition) ++failures;
	}

	std::string read(const std::string& path)
	{
		std::ifstream in(path);
		std::stringstream text;
		text << in.rdbuf();
		return text.str();
	}

	// The made-up world: places of a type, each a distance from the player; what a quest spawns is a place too
	struct World : Binding
	{
		struct Place { std::string type; double distance = 100.0; bool alive = true; };
		std::map<uint64_t, Place> places;
		std::map<std::string, double> flags;
		uint64_t next = 1000;
		void find(const AliasRule& rule, std::vector<Ref>& out) const override
		{
			for (const auto& [id, p] : places) if (p.type == rule.type && p.alive) out.push_back({ "place", id });
		}
		bool alive(const Ref& ref) const override { auto f = places.find(ref.id); return f != places.end() && f->second.alive; }
		double distance(const Ref& ref) const override { auto f = places.find(ref.id); return f != places.end() ? f->second.distance : -1.0; }
		std::string persist(const Ref& ref) const override { return "place:" + std::to_string(ref.id); }
		Ref restore(const std::string& key) const override { return { "place", std::stoull(key.substr(6)) }; }
		Ref spawn(const SpawnRule& rule, const Ref&) override { places[next] = { rule.type, 20.0 }; return { "place", next++ }; }
		void despawn(const Ref& ref) override { places[ref.id].alive = false; }
		double flag(const std::string& name) const override { auto f = flags.find(name); return f != flags.end() ? f->second : 0.0; }
		void set_flag(const std::string& name, double value) override { flags[name] = value; }
	};

	// The stand-in player: does what the current stage's first exit asks
	void play(QuestSystem& quests, World& world, const QuestRecord& record, const Quest& quest)
	{
		const Stage* stage = record.stage(quest.stage);
		if (!stage || stage->exits.empty()) return;
		const Exit& exit = stage->exits.front();
		auto near = [&](const std::string& alias, double meters)
		{
			auto held = quest.aliases.find(alias);
			if (held != quest.aliases.end()) world.places[held->second.id].distance = meters;
		};
		for (const std::string& item : exit.needs)
			if (!quests.items().count(item)) quests.set_item(item, 1);   // (a record played alone: what an earlier one gives)
		if (!exit.whenFlag.empty()) world.flags[exit.whenFlag] = exit.atLeast;
		if (!exit.whenVar.empty()) quests.set_var(quest.serial, exit.whenVar, exit.atLeast);
		if (exit.reachMeters >= 0.0) near(exit.reachAlias, exit.reachMeters / 2.0);
		if (exit.holdSeconds > 0.0) near(exit.holdAlias, exit.holdMeters / 2.0);
		if (exit.cleared)
		{
			for (const SpawnRule& rule : stage->spawns) near(rule.at, 10.0);   // (a spawn is made with the player near)
			for (const auto& [ref, source] : quest.spawned) world.places[ref.id].alive = false;
		}
		if (exit.useMeters >= 0.0)
		{
			near(exit.useAlias, exit.useMeters / 2.0);
			for (const QuestSystem::Offer& o : quests.offers()) if (o.serial == quest.serial && o.exit == 0) quests.use(o.serial, o.stage, 0);
		}
		if (!exit.chooseOption.empty())
			for (const QuestSystem::Choice& c : quests.choices()) if (c.serial == quest.serial) quests.choose(c.serial, c.stage, exit.chooseOption);
	}
}

int main(int argc, char** argv)
{
	const std::string dir = argc > 1 ? argv[1] : "game_genres/quests/templates";
	QuestSettings settings;
	std::vector<std::string> errors;
	check(read_settings(Json::parse(read(dir + "/settings.json"), nullptr, false), settings, errors) && errors.empty(), "the templates' settings read");
	std::map<std::string, QuestRecord> all;
	std::map<std::string, std::vector<std::string>> byFile;
	for (const char* file : { "fetch_and_fit.json", "choice_branch.json", "places.json" })
	{
		const Json data = Json::parse(read(dir + "/" + file), nullptr, false);
		check(data.is_object() && data.contains("note") && !data.value("records", Json::array()).empty(), std::string(file) + ": records and a note for the designer");
		for (const Json& json : data.value("records", Json::array()))
		{
			QuestRecord record;
			std::vector<std::string> readErrors;
			const bool read = read_record(json, record, readErrors);
			for (const std::string& e : readErrors) std::printf("     %s\n", e.c_str());
			const std::vector<std::string> findings = check_record(record, settings, {});
			for (const std::string& f : findings) std::printf("     %s\n", f.c_str());
			check(read && findings.empty(), std::string(file) + ": " + record.id + " reads and checks clean");
			all[record.id] = record;
			byFile[file].push_back(record.id);
		}
	}
	const std::vector<std::string> chains = check_chains(all);
	for (const std::string& f : chains) std::printf("     %s\n", f.c_str());
	check(chains.empty(), "the templates' chains, starts and items are sound");

	// each file played to its end: its records that nothing else starts are started, the rest come by 'then' and 'starts'
	for (const auto& [file, ids] : byFile)
	{
		World world;
		for (const char* type : { "village", "workshop", "camp" })
			for (int i = 0; i < 3; ++i) world.places[world.next++] = { type, 100.0 + 50.0 * i };   // (one for each record of a file)
		world.flags["world.count"] = 1.0;   // (a 'while' the reach template keeps)
		Arbiter arbiter;
		QuestSystem quests(arbiter, world, 7);
		quests.set_settings(settings);
		std::set<std::string> started;
		for (const std::string& id : ids) quests.add_record(all[id]);
		for (const std::string& id : ids)
			for (const Stage& stage : all[id].stages) for (const std::string& s : stage.starts) started.insert(s);
		for (const std::string& id : ids) if (!all[id].then.empty()) started.insert(all[id].then);
		for (const std::string& id : ids)
			if (!started.count(id)) check(quests.start(id, "template") != 0, file + ": " + id + " starts");
		for (uint64_t step = 1; step < 3000; ++step)
		{
			arbiter.begin_step(step);
			for (const auto& [serial, q] : quests.quests())
				if (q.state == "active") play(quests, world, all[q.record], q);
			quests.step(step, 0.5);
			arbiter.resolve(step);
		}
		for (const std::string& id : ids)
			check(quests.done_count(id) == 1, file + ": " + id + " played to its end");
	}
	std::printf("%s\n", failures ? "SOME CHECKS FAILED" : "all checks passed");
	return failures ? 1 : 0;
}
