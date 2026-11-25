#include "quests/QuestRecord.h"
#include <cstdio>

#include <deque>

namespace Pakal::Quests
{
	namespace
	{
		// MIS-V1: no field the format does not know (a typo would be silently ignored otherwise)
		void known_fields(const nlohmann::json& object, std::initializer_list<const char*> fields, const std::string& where,
			std::vector<std::string>& errors)
		{
			if (!object.is_object()) { errors.push_back(where + ": not an object"); return; }
			for (auto it = object.begin(); it != object.end(); ++it)
			{
				bool known = it.key() == "note";   // (a note for the designer is allowed anywhere)
				for (const char* field : fields) known = known || it.key() == field;
				if (!known) errors.push_back(where + ": unknown field '" + it.key() + "'");
			}
		}

		bool read_exit(const nlohmann::json& json, Exit& exit, bool needsTo, const std::string& where, std::vector<std::string>& errors)
		{
			known_fields(json, { "on", "when", "cleared", "fact", "reach", "past", "hold", "use", "needs", "gives", "choose", "to" }, where, errors);
			if (json.contains("choose"))
			{
				known_fields(json["choose"], { "option", "text" }, where + ".choose", errors);
				exit.chooseOption = json["choose"].value("option", std::string());
				exit.chooseText = json["choose"].value("text", std::string());
				if (exit.chooseOption.empty() || exit.chooseText.empty()) errors.push_back(where + ".choose: an option's id and its text");
			}
			if (json.contains("use"))
			{
				known_fields(json["use"], { "alias", "meters", "action" }, where + ".use", errors);
				exit.useAlias = json["use"].value("alias", std::string());
				exit.useMeters = json["use"].value("meters", 3.0);
				exit.useAction = json["use"].value("action", std::string("use"));
				if (exit.useAlias.empty() || exit.useMeters <= 0.0) errors.push_back(where + ".use: an alias and meters above 0");
			}
			auto read_items = [&](const char* field, std::vector<std::string>& out)
			{
				if (!json.contains(field)) return;
				const nlohmann::json& list = json[field];
				if (list.is_string()) out.push_back(list.get<std::string>());
				else if (list.is_array()) { for (const nlohmann::json& item : list) if (item.is_string() && !item.get<std::string>().empty()) out.push_back(item.get<std::string>()); }
				if (out.empty()) errors.push_back(where + "." + field + ": an item's name, or a list of them");
			};
			read_items("needs", exit.needs);
			read_items("gives", exit.gives);
			auto read_past = [&](const nlohmann::json& p, const std::string& at)
			{
				known_fields(p, { "axis", "value", "way" }, at, errors);
				exit.pastAxis = p.value("axis", std::string());
				exit.pastValue = p.value("value", 0.0);
				exit.pastWay = p.value("way", 1);
				if (exit.pastAxis.empty() || (exit.pastWay != 1 && exit.pastWay != -1)) errors.push_back(at + ": an axis and a way of 1 or -1");
			};
			if (json.contains("past")) read_past(json["past"], where + ".past");
			if (json.contains("hold"))
			{
				const nlohmann::json& h = json["hold"];
				known_fields(h, { "past", "alias", "meters", "seconds", "away", "awayGraceSeconds" }, where + ".hold", errors);
				if (h.contains("past")) read_past(h["past"], where + ".hold.past");
				if (h.contains("alias")) { exit.holdAlias = h.value("alias", std::string()); exit.holdMeters = h.value("meters", 6.0); }
				exit.holdSeconds = h.value("seconds", 0.0);
				exit.holdAway = h.value("away", std::string("pause"));
				exit.holdGrace = h.value("awayGraceSeconds", 0.0);
				if (exit.holdSeconds <= 0.0) errors.push_back(where + ".hold: seconds above 0");
				if (exit.pastAxis.empty() && exit.holdMeters < 0.0) errors.push_back(where + ".hold: a past line or an alias with meters");
				if (exit.holdAway != "pause" && exit.holdAway != "reset" && exit.holdAway != "fail") errors.push_back(where + ".hold: away '" + exit.holdAway + "' is not pause, reset or fail");
			}
			if (json.contains("reach"))
			{
				known_fields(json["reach"], { "alias", "meters" }, where + ".reach", errors);
				exit.reachAlias = json["reach"].value("alias", std::string());
				exit.reachMeters = json["reach"].value("meters", -1.0);
				if (exit.reachAlias.empty() || exit.reachMeters < 0.0) errors.push_back(where + ".reach: an alias and meters");
			}
			exit.cleared = json.value("cleared", false);
			if (json.contains("fact"))
			{
				known_fields(json["fact"], { "alias", "name" }, where + ".fact", errors);
				exit.factAlias = json["fact"].value("alias", std::string());
				exit.fact = json["fact"].value("name", std::string());
				if (exit.factAlias.empty() || exit.fact.empty()) errors.push_back(where + ".fact: an alias and a name");
			}
			if (json.contains("on"))
			{
				const nlohmann::json& on = json["on"];
				known_fields(on, { "signal", "alias", "count" }, where + ".on", errors);
				exit.signal = on.value("signal", std::string());
				exit.alias = on.value("alias", std::string());
				exit.count = on.value("count", 1);
				if (exit.signal.empty()) errors.push_back(where + ".on: no signal");
				if (exit.count < 1) errors.push_back(where + ".on: a count below 1");
			}
			if (json.contains("when"))
			{
				const nlohmann::json& when = json["when"];
				known_fields(when, { "var", "flag", "atLeast" }, where + ".when", errors);
				exit.whenVar = when.value("var", std::string());
				exit.whenFlag = when.value("flag", std::string());
				exit.atLeast = when.value("atLeast", 0.0);
				if (exit.whenVar.empty() == exit.whenFlag.empty()) errors.push_back(where + ".when: a var or a flag (one)");
			}
			if (exit.signal.empty() && exit.whenVar.empty() && exit.whenFlag.empty() && !exit.cleared && exit.fact.empty() && exit.reachMeters < 0.0
				&& exit.pastAxis.empty() && exit.holdSeconds <= 0.0 && exit.useMeters < 0.0 && exit.needs.empty() && exit.chooseOption.empty())
				errors.push_back(where + ": none of 'on', 'when', 'cleared', 'fact', 'reach', 'past', 'hold', 'use', 'choose'");
			if (!exit.chooseOption.empty() && (!exit.signal.empty() || exit.useMeters >= 0.0))
				errors.push_back(where + ": a 'choose' is the player's answer, not joined with a signal or a use");
			if (needsTo)
			{
				if (!json.contains("to") || !json["to"].is_number_integer()) errors.push_back(where + ": no stage to go 'to'");
				else exit.to = json["to"].get<int>();
			}
			return true;
		}
	}

	namespace
	{
		SpawnRule read_spawn(const nlohmann::json& sp, const std::string& sat, std::vector<std::string>& errors)
		{
			known_fields(sp, { "source", "kind", "type", "at", "count", "teardown", "near" }, sat, errors);
			SpawnRule rule;
			rule.source = sp.value("source", std::string());
			rule.kind = sp.value("kind", std::string("enemy"));
			rule.type = sp.value("type", std::string());
			rule.at = sp.value("at", std::string());
			rule.teardown = sp.value("teardown", std::string("remove"));
			rule.near = sp.value("near", 0.0);
			if (rule.teardown != "remove" && rule.teardown != "release") errors.push_back(sat + ": teardown '" + rule.teardown + "' is not remove or release");
			if (sp.contains("count") && sp["count"].is_array() && sp["count"].size() == 2)
			{
				rule.countMin = sp["count"][0].get<int>();
				rule.countMax = sp["count"][1].get<int>();
			}
			else rule.countMin = rule.countMax = sp.value("count", 1);
			if (rule.source.empty() || rule.type.empty() || rule.at.empty()) errors.push_back(sat + ": a source, a type and an alias to spawn 'at'");
			if (rule.countMin < 1 || rule.countMax < rule.countMin) errors.push_back(sat + ": a count of at least 1");
			return rule;
		}

		void read_while(const nlohmann::json& json, Exit& out, const std::string& where, std::vector<std::string>& errors)
		{
			read_exit(json, out, false, where, errors);
			if (!out.signal.empty() || out.cleared) errors.push_back(where + ": a 'while' holds a 'when' or a 'fact', not a signal or 'cleared'");
			if (out.useMeters >= 0.0 || !out.needs.empty() || !out.gives.empty() || !out.chooseOption.empty()) errors.push_back(where + ": a 'while' holds a 'when' or a 'fact', not a use, a choice or items");
		}
	}

	const Stage* QuestRecord::stage(int id) const
	{
		for (const Stage& s : stages) if (s.id == id) return &s;
		return nullptr;
	}

	bool read_record(const nlohmann::json& json, QuestRecord& out, std::vector<std::string>& errors)
	{
		const size_t before = errors.size();
		const std::string where = "quest '" + (json.is_object() ? json.value("id", std::string("?")) : std::string("?")) + "'";
		known_fields(json, { "format", "id", "version", "title", "brief", "kind", "priority", "repeatable", "starts", "onFail",
			"limitSeconds", "aliases", "stages", "fail", "while", "then", "scope", "concurrent", "game" }, where, errors);
		if (!json.is_object()) return false;
		out = QuestRecord();
		out.format = json.value("format", 1);
		if (out.format != 1) errors.push_back(where + ": format " + std::to_string(out.format) + " is not one this reader knows");
		out.id = json.value("id", std::string());
		if (out.id.empty()) errors.push_back(where + ": no id");
		out.version = json.value("version", 1);
		out.title = json.value("title", std::string());
		out.brief = json.value("brief", std::string());
		out.kind = json.value("kind", std::string());
		if (out.kind.empty()) errors.push_back(where + ": no kind");
		out.priority = json.value("priority", -1);
		out.repeatable = json.value("repeatable", false);
		out.onFail = json.value("onFail", std::string());
		out.then = json.value("then", std::string());
		out.concurrent = json.value("concurrent", false);
		if (out.concurrent && !json.value("repeatable", false)) errors.push_back(where + ": 'concurrent' without 'repeatable'");
		out.scope = json.value("scope", std::string("world"));
		if (out.scope != "world" && out.scope != "level") errors.push_back(where + ": scope '" + out.scope + "' is not world or level");
		out.limitSeconds = json.value("limitSeconds", 0.0);
		out.game = json.value("game", nlohmann::json::object());
		if (json.contains("starts"))
		{
			known_fields(json["starts"], { "by", "weight" }, where + ".starts", errors);
			out.startsBy = json["starts"].value("by", std::string("generator"));
			out.weight = json["starts"].value("weight", 1.0);
		}
		const nlohmann::json aliasesJson = json.value("aliases", nlohmann::json::object());
		for (auto it = aliasesJson.begin(); it != aliasesJson.end(); ++it)
		{
			const std::string at = where + ".aliases." + it.key();
			known_fields(it.value(), { "find", "type", "pick", "name", "where", "reserve", "protect", "lost", "repick", "optional" }, at, errors);
			AliasRule rule;
			rule.find = it.value().value("find", std::string());
			rule.type = it.value().value("type", std::string());
			rule.pick = it.value().value("pick", std::string("nearest"));
			rule.name = it.value().value("name", std::string());
			rule.reserve = it.value().value("reserve", true);
			rule.protect = it.value().value("protect", false);
			rule.lost = it.value().value("lost", std::string("fail"));
			rule.repick = it.value().value("repick", false);
			rule.optional = it.value().value("optional", false) || rule.repick;
			if (rule.repick && rule.pick == "named") errors.push_back(at + ": 'repick' on a named alias (there is one candidate)");
			if (rule.lost != "fail" && rule.lost != "refill") errors.push_back(at + ": lost '" + rule.lost + "' is not fail or refill");
			rule.where = it.value().value("where", nlohmann::json::object());
			if (rule.where.contains("meters") && rule.where["meters"].is_array() && rule.where["meters"].size() == 2)
			{
				rule.minMeters = rule.where["meters"][0].get<double>();
				rule.maxMeters = rule.where["meters"][1].get<double>();
			}
			if (rule.find.empty()) errors.push_back(at + ": no 'find'");
			if (rule.pick != "nearest" && rule.pick != "random" && rule.pick != "named") errors.push_back(at + ": pick '" + rule.pick + "' unknown");
			if (rule.pick == "named" && rule.name.empty()) errors.push_back(at + ": pick 'named' without a name");
			out.aliases[it.key()] = rule;
		}
		int index = 0;
		for (const nlohmann::json& s : json.value("stages", nlohmann::json::array()))
		{
			const std::string at = where + ".stages[" + std::to_string(index++) + "]";
			known_fields(s, { "id", "objectives", "marker", "exits", "end", "reward", "checkpoint", "spawns", "events", "while", "set", "starts", "choice", "game" }, at, errors);
			Stage stage;
			if (!s.contains("id") || !s["id"].is_number_integer()) { errors.push_back(at + ": no integer id"); continue; }
			stage.id = s["id"].get<int>();
			for (const nlohmann::json& line : s.value("objectives", nlohmann::json::array())) if (line.is_string()) stage.objectives.push_back(line.get<std::string>());
			stage.marker = s.value("marker", std::string());
			stage.end = s.value("end", std::string());
			stage.reward = s.value("reward", std::string());
			stage.checkpoint = s.value("checkpoint", false);
			int k = 0;
			for (const nlohmann::json& sp : s.value("spawns", nlohmann::json::array()))
				stage.spawns.push_back(read_spawn(sp, at + ".spawns[" + std::to_string(k++) + "]", errors));
			int v = 0;
			for (const nlohmann::json& ev : s.value("events", nlohmann::json::array()))
			{
				const std::string eat = at + ".events[" + std::to_string(v++) + "]";
				known_fields(ev, { "at", "signal", "spawn" }, eat, errors);
				TimedEvent event;
				event.at = ev.value("at", -1.0);
				event.signal = ev.value("signal", std::string());
				if (ev.contains("spawn")) event.spawns.push_back(read_spawn(ev["spawn"], eat + ".spawn", errors));
				if (event.at < 0.0) errors.push_back(eat + ": no 'at' (seconds into the stage)");
				if (event.signal.empty() && event.spawns.empty()) errors.push_back(eat + ": neither a signal nor a spawn");
				stage.events.push_back(event);
			}
			const nlohmann::json sets = s.value("set", nlohmann::json::object());   // (one object: its begin and end)
			for (auto f = sets.begin(); f != sets.end(); ++f)
				if (f.value().is_number()) stage.set[f.key()] = f.value().get<double>();
				else errors.push_back(at + ".set." + f.key() + ": not a number");
			if (s.contains("choice"))
			{
				known_fields(s["choice"], { "prompt", "speaker" }, at + ".choice", errors);
				stage.choicePrompt = s["choice"].value("prompt", std::string());
				stage.choiceSpeaker = s["choice"].value("speaker", std::string());
				if (stage.choicePrompt.empty()) errors.push_back(at + ".choice: its prompt (a text)");
			}
			if (s.contains("starts"))
			{
				const nlohmann::json& list = s["starts"];
				if (list.is_string()) stage.starts.push_back(list.get<std::string>());
				else if (list.is_array()) { for (const nlohmann::json& id : list) if (id.is_string() && !id.get<std::string>().empty()) stage.starts.push_back(id.get<std::string>()); }
				if (stage.starts.empty()) errors.push_back(at + ".starts: a record's id, or a list of them");
			}
			if (s.contains("while")) { stage.hasWhile = true; read_while(s["while"], stage.whileCond, at + ".while", errors); }
			if (!stage.end.empty() && stage.end != "complete" && stage.end != "fail" && stage.end != "abandon") errors.push_back(at + ": end '" + stage.end + "' unknown");
			int e = 0;
			for (const nlohmann::json& x : s.value("exits", nlohmann::json::array()))
			{
				Exit exit;
				read_exit(x, exit, true, at + ".exits[" + std::to_string(e++) + "]", errors);
				stage.exits.push_back(exit);
			}
			if (out.stage(stage.id)) errors.push_back(at + ": the stage id " + std::to_string(stage.id) + " twice");
			out.stages.push_back(stage);
		}
		if (out.stages.empty()) errors.push_back(where + ": no stages");
		index = 0;
		for (const nlohmann::json& f : json.value("fail", nlohmann::json::array()))
		{
			Exit exit;
			read_exit(f, exit, false, where + ".fail[" + std::to_string(index) + "]", errors);
			if (exit.useMeters >= 0.0 || !exit.needs.empty() || !exit.gives.empty() || !exit.chooseOption.empty())
				errors.push_back(where + ".fail[" + std::to_string(index) + "]: a fail waits on the world, not on a use, a choice or items");
			++index;
			out.fail.push_back(exit);
		}
		if (json.contains("while")) { out.hasWhile = true; read_while(json["while"], out.whileCond, where + ".while", errors); }
		return errors.size() == before;
	}

	bool read_settings(const nlohmann::json& json, QuestSettings& out, std::vector<std::string>& errors)
	{
		const size_t before = errors.size();
		known_fields(json, { "text", "kinds", "signals", "flags", "items", "limits", "beaten", "spawnMeters", "despawnMeters" }, "settings", errors);
		if (!json.is_object()) return false;
		out = QuestSettings();
		out.textMode = json.value("text", std::string("keys"));
		if (out.textMode != "keys" && out.textMode != "source") errors.push_back("settings: text '" + out.textMode + "' is not keys or source");
		const nlohmann::json kindsJson = json.value("kinds", nlohmann::json::object());
		for (auto it = kindsJson.begin(); it != kindsJson.end(); ++it)
			out.kindPriority[it.key()] = it.value().value("priority", 50);
		const nlohmann::json signalsJson = json.value("signals", nlohmann::json::object());
		for (auto it = signalsJson.begin(); it != signalsJson.end(); ++it)
			out.signals[it.key()] = it.value().get<std::string>();
		const nlohmann::json flagsJson = json.value("flags", nlohmann::json::object());
		for (auto it = flagsJson.begin(); it != flagsJson.end(); ++it)
			out.flags[it.key()] = it.value().get<std::string>();
		const nlohmann::json itemsJson = json.value("items", nlohmann::json::object());
		for (auto it = itemsJson.begin(); it != itemsJson.end(); ++it)
		{
			if (it.value().is_string()) out.items[it.key()] = it.value().get<std::string>();
			else errors.push_back("settings.items." + it.key() + ": its text");
		}
		const nlohmann::json limitsJson = json.value("limits", nlohmann::json::object());
		for (auto it = limitsJson.begin(); it != limitsJson.end(); ++it)
			out.limits[it.key()] = it.value().get<int>();
		out.spawnMeters = json.contains("spawnMeters") && json["spawnMeters"].is_number() ? json["spawnMeters"].get<double>() : 0.0;
		out.despawnMeters = json.contains("despawnMeters") && json["despawnMeters"].is_number() ? json["despawnMeters"].get<double>() : 0.0;
		if (json.contains("beaten"))
		{
			out.beaten.clear();
			for (const nlohmann::json& name : json["beaten"]) if (name.is_string()) out.beaten.insert(name.get<std::string>());
		}
		return errors.size() == before;
	}

	std::vector<std::string> check_record(const QuestRecord& record, const QuestSettings& settings, const CheckContext& context)
	{
		std::vector<std::string> out;
		const std::string where = "quest '" + record.id + "'";
		auto text = [&](const std::string& t, const std::string& at)
		{
			if (!t.empty() && context.textKnown && !context.textKnown(t)) out.push_back(at + ": the text '" + t + "' is not translated");
		};
		text(record.title, where + ".title");
		text(record.brief, where + ".brief");
		if (record.priority < 0 && !settings.kindPriority.count(record.kind))
			out.push_back(where + ": no priority, and its kind '" + record.kind + "' has none in the settings");
		for (const auto& [name, rule] : record.aliases)
			if (!context.aliasKinds.count(rule.find)) out.push_back(where + ".aliases." + name + ": a kind the binding does not resolve ('" + rule.find + "')");
		// MIS-V4: every signal a stage or a fail waits on is in the catalog (with an emitter)
		auto exit_ok = [&](const Exit& exit, const std::string& at)
		{
			if (!exit.signal.empty() && !settings.signals.count(exit.signal)) out.push_back(at + ": the signal '" + exit.signal + "' is not in the catalog");
			for (const std::string* alias : { &exit.alias, &exit.factAlias, &exit.reachAlias, &exit.holdAlias, &exit.useAlias })
				if (!alias->empty() && !record.aliases.count(*alias)) out.push_back(at + ": the alias '" + *alias + "' is not the quest's");
			if (!exit.whenFlag.empty() && !settings.flags.count(exit.whenFlag)) out.push_back(at + ": the flag '" + exit.whenFlag + "' is not in the catalog");
			// an item is in the catalog: the game shows it by its text
			for (const std::vector<std::string>* list : { &exit.needs, &exit.gives })
				for (const std::string& item : *list)
				{
					if (!settings.items.count(item)) out.push_back(at + ": the item '" + item + "' is not in the catalog");
					else text(settings.items.at(item), at + ": the item '" + item + "'");
				}
		};
		if (record.hasWhile) exit_ok(record.whileCond, where + ".while");
		for (const Stage& stage : record.stages)
		{
			const std::string at = where + ".stage " + std::to_string(stage.id);
			for (const std::string& line : stage.objectives) text(line, at);
			if (!stage.marker.empty() && !record.aliases.count(stage.marker)) out.push_back(at + ": marks the alias '" + stage.marker + "', not the quest's");
			if (!stage.reward.empty() && !context.rewards.empty() && !context.rewards.count(stage.reward)) out.push_back(at + ": the reward '" + stage.reward + "' does not exist");
			if (!stage.end.empty() && !stage.exits.empty()) out.push_back(at + ": a final stage with exits");
			if (stage.end.empty() && stage.exits.empty()) out.push_back(at + ": not final and no exit: a dead end");
			for (const SpawnRule& rule : stage.spawns)
				if (!record.aliases.count(rule.at)) out.push_back(at + ": spawns at '" + rule.at + "', not the quest's alias");
			if (stage.hasWhile) exit_ok(stage.whileCond, at + ".while");
			for (const auto& [name, value] : stage.set)
				if (!settings.flags.count(name)) out.push_back(at + ": sets the flag '" + name + "', which is not in the catalog");
			for (const TimedEvent& event : stage.events)
			{
				if (!event.signal.empty() && !settings.signals.count(event.signal)) out.push_back(at + ": an event's signal '" + event.signal + "' is not in the catalog");
				for (const SpawnRule& rule : event.spawns)
					if (!record.aliases.count(rule.at)) out.push_back(at + ": an event spawns at '" + rule.at + "', not the quest's alias");
			}
			// a choice: asked by its prompt, each option an id of its own with its text; a 'choose' needs the stage's 'choice'
			{
				std::set<std::string> options;
				int chooses = 0;
				for (const Exit& exit : stage.exits)
				{
					if (exit.chooseOption.empty()) continue;
					++chooses;
					text(exit.chooseText, at + ": the option '" + exit.chooseOption + "'");
					if (!options.insert(exit.chooseOption).second) out.push_back(at + ": the option '" + exit.chooseOption + "' twice");
				}
				if (chooses && stage.choicePrompt.empty()) out.push_back(at + ": options to choose and no 'choice' asking them");
				if (!chooses && !stage.choicePrompt.empty()) out.push_back(at + ": a 'choice' with no option ('choose' exits)");
				if (chooses == 1) out.push_back(at + ": a choice of one option");
				text(stage.choicePrompt, at + ".choice");
				if (!stage.choiceSpeaker.empty() && !record.aliases.count(stage.choiceSpeaker)) out.push_back(at + ".choice: the speaker '" + stage.choiceSpeaker + "' is not the quest's alias");
			}
			for (const Exit& exit : stage.exits)
			{
				exit_ok(exit, at);
				bool spawns = !stage.spawns.empty();
				for (const TimedEvent& event : stage.events) spawns = spawns || !event.spawns.empty();
				if (exit.cleared && !spawns) out.push_back(at + ": waits for its spawns cleared, and spawns nothing");
				if (!record.stage(exit.to)) out.push_back(at + ": goes to the stage " + std::to_string(exit.to) + ", which does not exist");
			}
		}
		for (const Exit& exit : record.fail) exit_ok(exit, where + ".fail");
		// MIS-V3: from the first stage, every stage reachable, and from every reachable stage a way to an end
		if (!record.stages.empty())
		{
			std::set<int> reached = { record.stages.front().id };
			std::deque<int> queue = { record.stages.front().id };
			while (!queue.empty())
			{
				const Stage* stage = record.stage(queue.front());
				queue.pop_front();
				if (!stage) continue;
				for (const Exit& exit : stage->exits) if (reached.insert(exit.to).second) queue.push_back(exit.to);
			}
			for (const Stage& stage : record.stages)
				if (!reached.count(stage.id)) out.push_back(where + ".stage " + std::to_string(stage.id) + ": never reached from the first stage");
			for (int id : reached)
			{
				std::set<int> seen = { id };
				std::deque<int> walk = { id };
				bool ends = false;
				while (!walk.empty() && !ends)
				{
					const Stage* stage = record.stage(walk.front());
					walk.pop_front();
					if (!stage) continue;
					if (!stage->end.empty()) { ends = true; break; }
					for (const Exit& exit : stage->exits) if (seen.insert(exit.to).second) walk.push_back(exit.to);
				}
				if (!ends) out.push_back(where + ".stage " + std::to_string(id) + ": no way from it to an end");
			}
		}
		// a story quest never stops (section 6): its fail leads somewhere, and it is not abandoned
		if (record.startsBy == "story" && record.onFail.empty()) out.push_back(where + ": a story quest with no onFail (retry, checkpoint or a follow-up)");
		// a protected alias is kept from the world, never from the player: one a stage wants destroyed or defeated (a fact
		// of that sort, a beaten signal about it) cannot be protected
		for (const Stage& stage : record.stages)
			for (const Exit& exit : stage.exits)
			{
				auto protected_alias = [&](const std::string& alias) { return !alias.empty() && record.aliases.count(alias) && record.aliases.at(alias).protect; };
				std::string alias;
				if (protected_alias(exit.factAlias) && (exit.fact == "destroyed" || exit.fact == "defeated" || exit.fact == "killed" || exit.fact == "dead")) alias = exit.factAlias;
				else if (protected_alias(exit.alias) && !exit.signal.empty() && settings.beaten.count(exit.signal)) alias = exit.alias;
				if (!alias.empty()) out.push_back(where + ".stages " + std::to_string(stage.id) + ": the alias '" + alias + "' is protected, and the player must destroy or defeat it");
			}
		// MIS-V6: a story quest's every alias is protected, or refilled when lost
		if (record.startsBy == "story")
			for (const auto& [name, rule] : record.aliases)
				if (!rule.protect && rule.lost != "refill" && !rule.repick) out.push_back(where + ".aliases." + name + ": a story quest's alias neither protected nor refilled when lost");
		return out;
	}

	std::vector<std::string> check_chains(const std::map<std::string, QuestRecord>& records)
	{
		std::vector<std::string> out;
		auto next = [&](const QuestRecord& r)
		{
			std::vector<std::string> to;
			if (!r.then.empty()) to.push_back(r.then);
			if (!r.onFail.empty() && r.onFail != "retry" && r.onFail != "checkpoint") to.push_back(r.onFail);
			return to;
		};
		for (const auto& [id, record] : records)
			for (const std::string& to : next(record))
				if (!records.count(to)) out.push_back("record " + id + ": its then/onFail names '" + to + "', which is no record");
		for (const auto& [id, record] : records)
			for (const Stage& stage : record.stages)
				for (const std::string& started : stage.starts)
				{
					if (!records.count(started)) out.push_back("record " + id + ".stage " + std::to_string(stage.id) + ": starts '" + started + "', which is no record");
					else if (started == id) out.push_back("record " + id + ".stage " + std::to_string(stage.id) + ": starts itself");
				}
		// an item needed is given somewhere: else no player could ever carry it
		std::set<std::string> given;
		for (const auto& [id, record] : records)
			for (const Stage& stage : record.stages)
				for (const Exit& exit : stage.exits) given.insert(exit.gives.begin(), exit.gives.end());
		for (const auto& [id, record] : records)
			for (const Stage& stage : record.stages)
				for (const Exit& exit : stage.exits)
					for (const std::string& item : exit.needs)
						if (!given.count(item)) out.push_back("record " + id + ".stage " + std::to_string(stage.id) + ": needs the item '" + item + "', which no record gives");
		// a cycle: walk from each record; one seen again on the same walk closes a loop
		for (const auto& [id, record] : records)
		{
			std::vector<std::string> walk = { id };
			std::set<std::string> onWalk = { id };
			std::vector<std::pair<std::string, size_t>> stack = { { id, 0 } };
			bool found = false;
			while (!stack.empty() && !found)
			{
				auto& [at, i] = stack.back();
				auto it = records.find(at);
				const std::vector<std::string> to = it != records.end() ? next(it->second) : std::vector<std::string>();
				if (i >= to.size()) { onWalk.erase(at); walk.pop_back(); stack.pop_back(); continue; }
				const std::string n = to[i++];
				if (n == id) { found = true; break; }
				if (!records.count(n) || onWalk.count(n)) continue;
				onWalk.insert(n);
				walk.push_back(n);
				stack.push_back({ n, 0 });
			}
			if (found)
			{
				std::string loop;
				for (const std::string& w : walk) loop += w + " -> ";
				out.push_back("record " + id + ": its then/onFail chain comes back to it (" + loop + id + ")");
			}
		}
		return out;
	}

	std::string describe_exit(const Exit& exit)
	{
		auto number = [](double v)
		{
			char buffer[32];
			std::snprintf(buffer, sizeof(buffer), "%g", v);
			return std::string(buffer);
		};
		std::vector<std::string> parts;
		if (!exit.signal.empty())
		{
			std::string part = "signal " + exit.signal;
			if (exit.count > 1) part += " x" + std::to_string(exit.count);
			if (!exit.alias.empty()) part += " of " + exit.alias;
			parts.push_back(part);
		}
		if (!exit.whenVar.empty()) parts.push_back("var " + exit.whenVar + " >= " + number(exit.atLeast));
		if (!exit.whenFlag.empty()) parts.push_back("flag " + exit.whenFlag + " >= " + number(exit.atLeast));
		if (!exit.fact.empty()) parts.push_back("fact " + exit.fact + " of " + exit.factAlias);
		if (exit.reachMeters >= 0.0) parts.push_back("reach " + exit.reachAlias + " within " + number(exit.reachMeters) + " m");
		if (!exit.pastAxis.empty()) parts.push_back("past " + exit.pastAxis + " " + number(exit.pastValue) + " going " + (exit.pastWay < 0 ? "-1" : "+1"));
		if (exit.holdSeconds > 0.0)
			parts.push_back("hold " + number(exit.holdSeconds) + " s " + (exit.holdMeters >= 0.0 ? "within " + number(exit.holdMeters) + " m of " + exit.holdAlias : std::string("past the line"))
				+ " (away: " + exit.holdAway + (exit.holdGrace > 0.0 ? ", " + number(exit.holdGrace) + " s grace" : std::string()) + ")");
		if (exit.cleared) parts.push_back("its spawns beaten");
		if (exit.useMeters >= 0.0) parts.push_back("use (" + exit.useAction + ") " + exit.useAlias + " within " + number(exit.useMeters) + " m");
		if (!exit.chooseOption.empty()) parts.push_back("the choice '" + exit.chooseOption + "'");
		auto list = [](const std::vector<std::string>& items)
		{
			std::string out;
			for (size_t i = 0; i < items.size(); ++i) out += (i ? ", " : "") + items[i];
			return out;
		};
		if (!exit.needs.empty()) parts.push_back("carrying " + list(exit.needs) + " (handed over)");
		std::string out;
		for (size_t i = 0; i < parts.size(); ++i) out += (i ? " and " : "") + parts[i];
		if (out.empty()) out = "nothing (always)";
		if (!exit.gives.empty()) out += "; gives " + list(exit.gives);
		return out;
	}
}
