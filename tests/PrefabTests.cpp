///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PrefabTests.cpp
// Purpose: Prefabs of a level: an object made from a prefab keeps only its overrides (saved and read
//          back), a variant merges over its base (null removes), the mesh comes relative to the level,
//          overrides set and revert by JSON pointer, a prefab made from an object and its overrides
//          applied to the prefab, a changed prefab file seen, and an old level saved as it was.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "LevelDocument.h"
#include "LevelEditor.h"
#include "PrefabLibrary.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace Pakal;

namespace
{
	int g_failures = 0;

	void check(bool condition, const char* what)
	{
		if (!condition)
		{
			++g_failures;
			std::printf("FAILED: %s\n", what);
		}
	}

	void write(const std::filesystem::path& path, const char* text)
	{
		std::filesystem::create_directories(path.parent_path());
		std::ofstream(path, std::ios::binary | std::ios::trunc) << text;
	}
}

int main()
{
	namespace fs = std::filesystem;
	const fs::path root = fs::temp_directory_path() / "pakal_prefab_tests";
	fs::remove_all(root);
	const fs::path levelPath = root / "level" / "level.json";
	write(root / "level" / "prefabs" / "Barrel.prefab.json", R"({
		"mesh": "../models/barrel.glb", "role": "prop", "tags": ["breakable"],
		"collision": { "shape": "circle", "d": 0.8 },
		"components": { "Destructible": { "health": 40 }, "Explosive": { "radius": 3, "damage": 80 } } })");
	write(root / "level" / "prefabs" / "SmallBarrel.prefab.json", R"({
		"base": "Barrel", "components": { "Destructible": { "health": 10 }, "Explosive": null } })");
	write(levelPath, R"({ "format": "pakal-level", "version": 2, "name": "t", "units_per_meter": 100, "spawn": [0, 0],
		"objects": [ { "key": "box_01", "name": "Box", "mesh": "models/box.glb", "x": 0, "y": 0, "z": 0, "rot": [0, 0, 0], "scale": 1 } ] })");

	std::string error;
	LevelDocument doc;
	check(doc.load(levelPath.string(), error), "the level loads");
	const std::string before = doc.to_json().dump();

	PrefabLibrary prefabs;
	prefabs.open(levelPath.string(), doc);
	check(prefabs.names() == std::vector<std::string>({ "Barrel", "SmallBarrel" }), "the folder's prefabs are listed");

	// a variant merges over its base, and its null removes
	Json small = prefabs.prefab("SmallBarrel", error);
	check(error.empty() && small["components"]["Destructible"]["health"] == 10 && !small["components"].contains("Explosive"),
		"a variant changes its base and removes with null");
	check(small["mesh"] == "models/barrel.glb" && small["role"] == "prop" && !small.contains("base"), "the mesh comes relative to the level");

	// an object made from a prefab, its overrides
	LevelEditor editor(doc);
	editor.set_prefabs(&prefabs);
	check(!editor.apply({ { "op", "instantiate" }, { "prefab", "Nothing" }, { "x", 1.0 }, { "y", 1.0 } }).empty(), "an unknown prefab is refused");
	check(editor.apply({ { "op", "instantiate" }, { "prefab", "Barrel" }, { "x", 2.0 }, { "y", 3.0 }, { "yaw", 90.0 } }).empty(), "a prefab is instantiated");
	const int barrel = doc.find("barrel_01");
	check(barrel == 1 && doc.objects[1].prefab == "Barrel" && doc.objects[1].mesh.empty() && std::fabs(doc.objects[1].x - 200.0) < 1e-9,
		"the object names its prefab, has a key and no model of its own");
	check(editor.apply({ { "op", "override" }, { "object", "barrel_01" }, { "path", "/components/Destructible/health" }, { "value", 55 } }).empty(),
		"an override is set");
	check(!editor.apply({ { "op", "override" }, { "object", "barrel_01" }, { "path", "components" }, { "value", 1 } }).empty(), "a path without '/' is refused");
	Json resolved = prefabs.resolve(doc.objects[1], error);
	check(error.empty() && resolved["components"]["Destructible"]["health"] == 55 && resolved["components"]["Explosive"]["radius"] == 3,
		"the override wins and the rest comes from the prefab");
	check(editor.apply({ { "op", "place" }, { "object", "barrel_01" }, { "z", 0.5 }, { "rot", Json::array({ 10, 0, 45 }) }, { "scale", 2.0 } }).empty()
		&& std::fabs(doc.objects[1].z - 50.0) < 1e-9 && doc.objects[1].rot[0] == 10.0 && doc.objects[1].scale == 2.0 && std::fabs(doc.objects[1].x - 200.0) < 1e-9,
		"place changes only what it is given");

	// saved and read back: only the overrides are in the level
	check(doc.save(levelPath.string(), error), "the level saves");
	LevelDocument again;
	check(again.load(levelPath.string(), error) && again.objects.size() == 2 && again.objects[1].prefab == "Barrel"
		&& again.objects[1].overrides == Json::parse(R"({"components":{"Destructible":{"health":55}}})"), "prefab and overrides are read back");
	const Json saved = again.to_json()["objects"][1];
	check(!saved.contains("mesh") && !saved.contains("components"), "the level keeps only the overrides, not the prefab");

	// revert one, then all
	check(editor.apply({ { "op", "revert" }, { "object", "barrel_01" }, { "path", "/components/Destructible/health" } }).empty()
		&& doc.objects[1].overrides.empty(), "reverting the only override leaves none (empty objects go too)");
	editor.undo();
	check(doc.objects[1].overrides["components"]["Destructible"]["health"] == 55, "undo gives the override back");

	// apply to the prefab: every object made from it changes
	check(editor.apply({ { "op", "apply-to-prefab" }, { "object", "barrel_01" } }).empty() && doc.objects[1].overrides.empty(),
		"the overrides go into the prefab");
	check(prefabs.prefab("Barrel", error)["components"]["Destructible"]["health"] == 55, "the prefab's file has them");
	check(prefabs.prefab("SmallBarrel", error)["components"]["Destructible"]["health"] == 10, "a variant keeps its own value");

	// a prefab made from a plain object
	check(editor.apply({ { "op", "override" }, { "object", "box_01" }, { "path", "/components/GiftBox" }, { "value", Json::object() } }).empty(),
		"a plain object can carry components");
	check(editor.apply({ { "op", "make-prefab" }, { "object", "box_01" }, { "name", "Crate" } }).empty(), "a prefab is made from an object");
	Json crate = prefabs.prefab("Crate", error);
	check(error.empty() && crate["mesh"] == "models/box.glb" && crate["components"].contains("GiftBox"), "it has the object's model and components");
	check(doc.objects[0].prefab == "Crate" && doc.objects[0].mesh.empty() && doc.objects[0].overrides.empty(), "and the object is made from it");
	check(!editor.apply({ { "op", "make-prefab" }, { "object", "box_01" }, { "name", "Crate" } }).empty(), "a prefab's name is not taken twice");

	// a variant made from an object made from a prefab holds only what differs
	editor.apply({ { "op", "override" }, { "object", "barrel_01" }, { "path", "/tags" }, { "value", Json::array({ "red" }) } });
	check(editor.apply({ { "op", "make-prefab" }, { "object", "barrel_01" }, { "name", "RedBarrel" } }).empty(), "a variant is made");
	const Json& red = prefabs.raw("RedBarrel");
	check(red["base"] == "Barrel" && red.contains("tags") && !red.contains("mesh") && !red.contains("components"), "the variant holds only its changes");

	// a prefab file changed outside is seen
	prefabs.prefab("Barrel", error);
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	write(root / "level" / "prefabs" / "Barrel.prefab.json", R"({ "mesh": "../models/barrel2.glb" })");
	fs::last_write_time(root / "level" / "prefabs" / "Barrel.prefab.json", fs::file_time_type::clock::now() + std::chrono::seconds(5));
	const auto changed = prefabs.refresh();
	check(std::find(changed.begin(), changed.end(), "Barrel") != changed.end() && prefabs.prefab("Barrel", error)["mesh"] == "models/barrel2.glb",
		"a changed prefab file is read again");

	// an old level without prefabs saves as it was
	LevelDocument old;
	old.from_json(Json::parse(before), error);
	check(old.to_json().dump() == before, "a level without prefabs is saved unchanged");

	// a converter's negative game id survives a save (only -1, none, is left out)
	LevelDocument negative;
	negative.from_json(Json::parse(R"({ "format": "pakal-level", "version": 2, "objects": [ { "name": "Glock", "id": -11, "mesh": "g.dae" } ] })"), error);
	LevelDocument back;
	back.from_json(negative.to_json(), error);
	check(back.objects.size() == 1 && back.objects[0].id == -11, "a negative game id is kept");

	// a model's own matrix (basis) survives a save; place keeps it with a new one, and angles or a scale replace it
	LevelDocument tilted;
	tilted.from_json(Json::parse(R"({ "format": "pakal-level", "version": 2, "objects": [ { "key": "wall_01", "mesh": "w.glb",
		"basis": [2, 0, 0, 0, 1, 0, 0, 0, 1] } ] })"), error);
	LevelDocument tiltedBack;
	tiltedBack.from_json(tilted.to_json(), error);
	check(tiltedBack.objects[0].has_basis && tiltedBack.objects[0].basis[0] == 2.0, "a basis is kept");
	LevelEditor tiltedEditor(tiltedBack);
	check(tiltedEditor.apply({ { "op", "place" }, { "object", "wall_01" }, { "x", 1.0 }, { "basis", Json::array({ 3, 0, 0, 0, 1, 0, 0, 0, 1 }) } }).empty()
		&& tiltedBack.objects[0].has_basis && tiltedBack.objects[0].basis[0] == 3.0, "place sets a basis");
	check(!tiltedEditor.apply({ { "op", "place" }, { "object", "wall_01" }, { "basis", Json::array({ 1, 2 }) } }).empty(), "a basis of other than nine is refused");
	check(tiltedEditor.apply({ { "op", "place" }, { "object", "wall_01" }, { "rot", Json::array({ 0, 0, 90 }) } }).empty() && !tiltedBack.objects[0].has_basis,
		"angles replace a basis");

	// diff is the smallest patch
	const Json a = Json::parse(R"({"x":1,"o":{"p":1,"q":2},"gone":true})"), b = Json::parse(R"({"x":1,"o":{"p":1,"q":3},"new":5})");
	Json patched = a;
	patched.merge_patch(PrefabLibrary::diff(a, b));
	check(patched == b && PrefabLibrary::diff(a, b) == Json::parse(R"({"o":{"q":3},"new":5,"gone":null})"), "diff gives the smallest merge patch");

	fs::remove_all(root);
	if (g_failures == 0) std::printf("PrefabTests: all passed\n");
	return g_failures == 0 ? 0 : 1;
}
