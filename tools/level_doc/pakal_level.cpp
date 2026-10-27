///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: pakal_level.cpp
// Purpose: pakal-level, a level's command line for people and agents: what a level holds (objects by
//          readable key, zones, roles), its operations (LevelEditor: move, turn, add, delete...), undo and
//          redo that last between runs (a <level>.history folder next to it), checks, and the level
//          split into files of its own per section. Positions are meters. A running game with live
//          reload on (ZOMBIES_LIVE_RELOAD=1) shows each change as it is saved.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "LevelDocument.h"
#include "LevelEditor.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

using namespace Pakal;

namespace
{
	void usage()
	{
		std::printf(
			"pakal-level <level.json> <command> [arguments] [--json]\n"
			"\n"
			"Looking:\n"
			"  info                         name, size, spawn, counts, zones, parts, history\n"
			"  list [--zone Z] [--role R] [--match TEXT] [--near X,Y,R] [--limit N]\n"
			"                               objects: key, name, x y z (m), yaw, scale, role, zone, model\n"
			"  show <object>                one object in full (an object: its key, #<index> or id:<game id>)\n"
			"  zones                        the zones and how many objects each holds\n"
			"  validate                     the level's problems (exit 1 when there is an error)\n"
			"  history                      the operations done and the ones that can be redone\n"
			"\n"
			"Changing (saved at once, undoable):\n"
			"  move <object> <x> <y> [z]    | nudge <object> <dx> <dy>   | turn <object> <degrees>   | scale <object> <s>\n"
			"  add <model> <x> <y> [key=K] [name=N] [role=R] [zone=Z] [yaw=D] [scale=S] [z=Z]\n"
			"  copy <object> <x> <y> [key]  | delete <object>            | set <object> <field> <value>\n"
			"  zone <name> <kind> <x0> <y0> <x1> <y1>    | unzone <name>  | spawn <x> <y>\n"
			"  keys                         a readable key for every object without one\n"
			"  zoning [all]                 each object's zone from where it stands\n"
			"  do '<json operation>'        any operation of LevelEditor (see 'ops')\n"
			"  undo | redo\n"
			"\n"
			"Files:\n"
			"  split [section...]           keep sections in files of their own (default: objects collision script)\n"
			"  join                         everything back in one file\n"
			"  ops                          the operations and their fields\n");
	}

	double meters(const LevelDocument& doc, double units) { return units / doc.unitsPerMeter; }

	Json object_json(const LevelDocument& doc, const LevelObject& o, int index)
	{
		Json item = { { "key", o.key }, { "index", index }, { "name", o.name }, { "x", meters(doc, o.x) }, { "y", meters(doc, o.y) },
			{ "z", meters(doc, o.z) }, { "yaw", o.rot[2] }, { "scale", o.scale }, { "mesh", o.mesh } };
		if (o.id >= 0) item["id"] = o.id;
		if (!o.role.empty()) item["role"] = o.role;
		if (!o.zone.empty()) item["zone"] = o.zone;
		return item;
	}

	std::string option(std::vector<std::string>& args, const std::string& name)
	{
		for (size_t i = 0; i + 1 < args.size(); ++i)
		{
			if (args[i] == name)
			{
				const std::string value = args[i + 1];
				args.erase(args.begin() + static_cast<long>(i), args.begin() + static_cast<long>(i) + 2);
				return value;
			}
		}
		return std::string();
	}

	bool flag(std::vector<std::string>& args, const std::string& name)
	{
		for (size_t i = 0; i < args.size(); ++i)
		{
			if (args[i] == name)
			{
				args.erase(args.begin() + static_cast<long>(i));
				return true;
			}
		}
		return false;
	}

	// "key=value" arguments of add
	void named_fields(const std::vector<std::string>& args, size_t from, Json& op)
	{
		for (size_t i = from; i < args.size(); ++i)
		{
			const size_t equals = args[i].find('=');
			if (equals == std::string::npos) continue;
			const std::string field = args[i].substr(0, equals), value = args[i].substr(equals + 1);
			if (field == "yaw" || field == "scale" || field == "z") op[field] = std::atof(value.c_str());
			else op[field] = value;
		}
	}
}

int main(int argc, char** argv)
{
	std::vector<std::string> args(argv + 1, argv + argc);
	if (args.size() < 2 || args[0] == "--help" || args[0] == "-h")
	{
		usage();
		return args.empty() ? 1 : 0;
	}
	const bool asJson = flag(args, "--json");
	const std::string path = args[0];
	const std::string command = args[1];
	if (command == "ops")
	{
		for (const auto& line : LevelEditor::help()) std::printf("%s\n", line.c_str());
		return 0;
	}

	LevelDocument doc;
	std::string error;
	if (!doc.load(path, error))
	{
		std::fprintf(stderr, "error: %s\n", error.c_str());
		return 1;
	}
	const std::filesystem::path file(path);
	const std::string historyFolder = (file.parent_path() / (file.stem().string() + ".history")).string();
	LevelEditor editor(doc);
	if (!editor.load_history(historyFolder, error))
	{
		std::fprintf(stderr, "error: %s\n", error.c_str());
		return 1;
	}
	auto save = [&]() -> int
	{
		if (!doc.save(path, error) || !editor.save_history(historyFolder, error))
		{
			std::fprintf(stderr, "error: %s\n", error.c_str());
			return 1;
		}
		return 0;
	};
	auto need = [&](size_t count) -> bool
	{
		if (args.size() >= count + 2) return true;
		std::fprintf(stderr, "error: '%s' needs %zu arguments (pakal-level --help)\n", command.c_str(), count);
		return false;
	};
	auto num = [&](size_t i) { return std::atof(args[i].c_str()); };

	// --- looking ---
	if (command == "info")
	{
		Json info = { { "name", doc.name }, { "mode", doc.mode }, { "unitsPerMeter", doc.unitsPerMeter }, { "objects", doc.objects.size() },
			{ "walls", doc.walls.size() }, { "zones", doc.zones.size() }, { "parts", doc.parts }, { "undo", editor.done().size() },
			{ "redo", editor.undone().size() } };
		if (doc.hasSpawn) info["spawn"] = Json::array({ meters(doc, doc.spawn[0]), meters(doc, doc.spawn[1]) });
		double x0 = 1e30, y0 = 1e30, x1 = -1e30, y1 = -1e30;
		int keyless = 0;
		for (const auto& o : doc.objects)
		{
			x0 = std::min(x0, o.x); y0 = std::min(y0, o.y); x1 = std::max(x1, o.x); y1 = std::max(y1, o.y);
			if (o.key.empty()) ++keyless;
		}
		if (!doc.objects.empty()) info["bounds"] = Json::array({ meters(doc, x0), meters(doc, y0), meters(doc, x1), meters(doc, y1) });
		info["withoutKey"] = keyless;
		std::printf("%s\n", info.dump(asJson ? -1 : 1).c_str());
		return 0;
	}
	if (command == "list")
	{
		const std::string zone = option(args, "--zone"), role = option(args, "--role"), match = option(args, "--match"), near = option(args, "--near");
		const std::string limitText = option(args, "--limit");
		const size_t limit = limitText.empty() ? 1000000 : static_cast<size_t>(std::atoi(limitText.c_str()));
		double nx = 0.0, ny = 0.0, nr = -1.0;
		if (!near.empty() && std::sscanf(near.c_str(), "%lf,%lf,%lf", &nx, &ny, &nr) != 3)
		{
			std::fprintf(stderr, "error: --near takes X,Y,R (meters)\n");
			return 1;
		}
		Json list = Json::array();
		for (size_t i = 0; i < doc.objects.size() && list.size() < limit; ++i)
		{
			const LevelObject& o = doc.objects[i];
			if (!zone.empty() && o.zone != zone) continue;
			if (!role.empty() && o.role != role) continue;
			if (!match.empty() && o.key.find(match) == std::string::npos && o.name.find(match) == std::string::npos && o.mesh.find(match) == std::string::npos) continue;
			if (nr >= 0.0 && std::hypot(meters(doc, o.x) - nx, meters(doc, o.y) - ny) > nr) continue;
			list.push_back(object_json(doc, o, static_cast<int>(i)));
		}
		if (asJson)
		{
			std::printf("%s\n", list.dump().c_str());
			return 0;
		}
		for (const auto& item : list)
		{
			std::printf("%-28s %-22s %8.2f %8.2f %6.2f %7.1f %5.2f  %-10s %-12s %s\n", item.value("key", std::string("#" + std::to_string(item["index"].get<int>()))).c_str(),
				item["name"].get<std::string>().c_str(), item["x"].get<double>(), item["y"].get<double>(), item["z"].get<double>(), item["yaw"].get<double>(),
				item["scale"].get<double>(), item.value("role", std::string("-")).c_str(), item.value("zone", std::string("-")).c_str(), item["mesh"].get<std::string>().c_str());
		}
		std::printf("%zu objects\n", list.size());
		return 0;
	}
	if (command == "show")
	{
		if (!need(1)) return 1;
		const int index = doc.find(args[2]);
		if (index < 0)
		{
			std::fprintf(stderr, "error: no object '%s'\n", args[2].c_str());
			return 1;
		}
		const LevelObject& o = doc.objects[static_cast<size_t>(index)];
		Json item = object_json(doc, o, index);
		item["rot"] = Json::array({ o.rot[0], o.rot[1], o.rot[2] });
		if (o.baked) item["baked"] = true;
		Json collision = Json::array();
		for (const auto& w : doc.walls)
		{
			if (o.id >= 0 && w.id == o.id)
			{
				collision.push_back({ { "shape", w.shape }, { "x", meters(doc, w.x) }, { "y", meters(doc, w.y) }, { "w", meters(doc, w.w) },
					{ "h", meters(doc, w.h) }, { "d", meters(doc, w.d) } });
			}
		}
		if (!collision.empty()) item["collision"] = collision;
		std::printf("%s\n", item.dump(asJson ? -1 : 1).c_str());
		return 0;
	}
	if (command == "zones")
	{
		Json zones = Json::array();
		for (const auto& z : doc.zones)
		{
			int count = 0;
			for (const auto& o : doc.objects) count += o.zone == z.name ? 1 : 0;
			zones.push_back({ { "name", z.name }, { "kind", z.kind }, { "rect", Json::array({ meters(doc, z.x0), meters(doc, z.y0), meters(doc, z.x1), meters(doc, z.y1) }) },
				{ "objects", count } });
		}
		if (asJson) std::printf("%s\n", zones.dump().c_str());
		else
		{
			for (const auto& z : zones)
			{
				std::printf("%-20s %-10s [%.1f, %.1f, %.1f, %.1f]  %d objects\n", z["name"].get<std::string>().c_str(), z["kind"].get<std::string>().c_str(),
					z["rect"][0].get<double>(), z["rect"][1].get<double>(), z["rect"][2].get<double>(), z["rect"][3].get<double>(), z["objects"].get<int>());
			}
			std::printf("%zu zones\n", zones.size());
		}
		return 0;
	}
	if (command == "validate")
	{
		const auto problems = doc.validate(LevelDocument::directory_of(path));
		bool errors = false;
		for (const auto& line : problems)
		{
			std::printf("%s\n", line.c_str());
			errors = errors || line.rfind("error:", 0) == 0;
		}
		if (problems.empty()) std::printf("ok\n");
		return errors ? 1 : 0;
	}
	if (command == "history")
	{
		Json history = { { "done", editor.done() }, { "redo", editor.undone() } };
		std::printf("%s\n", history.dump(asJson ? -1 : 1).c_str());
		return 0;
	}

	// --- changing ---
	if (command == "undo" || command == "redo")
	{
		const std::string what = command == "undo" ? editor.undo() : editor.redo();
		if (what.empty())
		{
			std::fprintf(stderr, "error: nothing to %s\n", command.c_str());
			return 1;
		}
		if (int failed = save()) return failed;
		std::printf("%s: %s\n", command == "undo" ? "undone" : "redone", what.c_str());
		return 0;
	}
	if (command == "split" || command == "join")
	{
		if (command == "join")
		{
			const std::vector<std::string> old = doc.parts;
			doc.parts.clear();
			if (int failed = save()) return failed;
			for (const auto& section : old) std::filesystem::remove(LevelDocument::part_path(path, section));
		}
		else
		{
			std::vector<std::string> sections(args.begin() + 2, args.end());
			if (sections.empty()) sections = { "objects", "collision", "script" };
			doc.parts = sections;
			if (int failed = save()) return failed;
		}
		std::printf("%s\n", Json(doc.parts).dump().c_str());
		return 0;
	}

	Json op;
	if (command == "do")
	{
		if (!need(1)) return 1;
		op = Json::parse(args[2], nullptr, false);
		if (op.is_discarded() || !op.is_object())
		{
			std::fprintf(stderr, "error: not a JSON object: %s\n", args[2].c_str());
			return 1;
		}
	}
	else if (command == "move")
	{
		if (!need(3)) return 1;
		op = { { "op", "move" }, { "object", args[2] }, { "x", num(3) }, { "y", num(4) } };
		if (args.size() > 5) op["z"] = num(5);
	}
	else if (command == "nudge")
	{
		if (!need(3)) return 1;
		op = { { "op", "move" }, { "object", args[2] }, { "dx", num(3) }, { "dy", num(4) } };
	}
	else if (command == "turn")
	{
		if (!need(2)) return 1;
		op = { { "op", "turn" }, { "object", args[2] }, { "yaw", num(3) } };
	}
	else if (command == "scale")
	{
		if (!need(2)) return 1;
		op = { { "op", "scale" }, { "object", args[2] }, { "scale", num(3) } };
	}
	else if (command == "add")
	{
		if (!need(3)) return 1;
		op = { { "op", "add" }, { "mesh", args[2] }, { "x", num(3) }, { "y", num(4) } };
		named_fields(args, 5, op);
	}
	else if (command == "copy")
	{
		if (!need(3)) return 1;
		op = { { "op", "copy" }, { "object", args[2] }, { "x", num(3) }, { "y", num(4) } };
		if (args.size() > 5) op["key"] = args[5];
	}
	else if (command == "delete")
	{
		if (!need(1)) return 1;
		op = { { "op", "delete" }, { "object", args[2] } };
	}
	else if (command == "set")
	{
		if (!need(3)) return 1;
		op = { { "op", "set" }, { "object", args[2] }, { "field", args[3] }, { "value", args[4] } };
	}
	else if (command == "zone")
	{
		if (!need(6)) return 1;
		op = { { "op", "zone" }, { "name", args[2] }, { "kind", args[3] }, { "rect", Json::array({ num(4), num(5), num(6), num(7) }) } };
	}
	else if (command == "unzone")
	{
		if (!need(1)) return 1;
		op = { { "op", "unzone" }, { "name", args[2] } };
	}
	else if (command == "spawn")
	{
		if (!need(2)) return 1;
		op = { { "op", "spawn" }, { "x", num(2) }, { "y", num(3) } };
	}
	else if (command == "keys")
	{
		op = { { "op", "keys" } };
	}
	else if (command == "zoning")
	{
		op = { { "op", "zoning" }, { "all", args.size() > 2 && args[2] == "all" } };
	}
	else
	{
		std::fprintf(stderr, "error: unknown command '%s' (pakal-level --help)\n", command.c_str());
		return 1;
	}
	const std::string failure = editor.apply(op);
	if (!failure.empty())
	{
		std::fprintf(stderr, "error: %s\n", failure.c_str());
		return 1;
	}
	if (int failed = save()) return failed;
	std::printf("done: %s\n", op.dump().c_str());
	return 0;
}
