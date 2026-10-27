///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelEditorTests.cpp
// Purpose: The level's editing for tools and agents: readable keys, operations in meters (an object's
//          collision moves with it), a failed operation that changes nothing, undo and redo (also kept
//          in a folder between runs), zones, and a level split into files per section and read back.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "LevelDocument.h"
#include "LevelEditor.h"

#include <cmath>
#include <cstdio>
#include <filesystem>

using namespace Pakal;

namespace
{
	LevelDocument sample()
	{
		LevelDocument doc;
		doc.hasSpawn = true;
		doc.spawn = { 0.0, 0.0 };
		LevelObject cone;
		cone.name = "Traffic Cone";
		cone.id = 7;
		cone.mesh = "meshes/cone.dae";
		cone.x = 100.0;
		cone.y = 200.0;
		doc.objects.push_back(cone);
		cone.id = -1;
		cone.x = 500.0;
		doc.objects.push_back(cone);
		LevelObject house;
		house.name = "Building House";
		house.mesh = "meshes/house.glb";
		house.x = 2000.0;
		house.y = 2000.0;
		doc.objects.push_back(house);
		LevelWall wall;
		wall.shape = "circle";
		wall.d = 50.0;
		wall.x = 100.0;
		wall.y = 200.0;
		wall.id = 7;
		doc.walls.push_back(wall);
		return doc;
	}

	bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }
}

int main()
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	int failures = 0;
	auto check = [&](bool ok, const char* message)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
		if (!ok) ++failures;
	};

	{
		LevelDocument doc = sample();
		check(doc.assign_keys() == 3 && doc.objects[0].key == "traffic_cone_01" && doc.objects[1].key == "traffic_cone_02"
			&& doc.objects[2].key == "building_house_01", "every object gets a readable key from its name");
		check(doc.find("traffic_cone_02") == 1 && doc.find("#2") == 2 && doc.find("id:7") == 0 && doc.find("nothing") == -1,
			"objects are found by key, by index and by game id");

		LevelEditor editor(doc);
		check(editor.apply({ { "op", "move" }, { "object", "traffic_cone_01" }, { "x", 3.0 }, { "y", 4.0 } }).empty()
			&& near(doc.objects[0].x, 300.0) && near(doc.objects[0].y, 400.0) && near(doc.walls[0].x, 300.0) && near(doc.walls[0].y, 400.0),
			"a move is in meters, and the object's collision goes with it");
		check(editor.apply({ { "op", "move" }, { "object", "traffic_cone_01" }, { "dx", -1.0 }, { "dy", 0.5 } }).empty()
			&& near(doc.objects[0].x, 200.0) && near(doc.objects[0].y, 450.0), "a nudge moves it by an amount");
		const std::string failed = editor.apply({ { "op", "set" }, { "object", "traffic_cone_02" }, { "field", "key" }, { "value", "traffic_cone_01" } });
		check(!failed.empty() && doc.objects[1].key == "traffic_cone_02", "a key that is taken is refused, and nothing changes");
		check(!editor.apply({ { "op", "fly" } }).empty() && !editor.apply({ { "op", "turn" }, { "object", "ghost" }, { "yaw", 1 } }).empty(),
			"an unknown operation or object is refused");

		check(editor.undo().find("dx") != std::string::npos && near(doc.objects[0].x, 300.0), "undo takes the last operation back");
		check(!editor.undo().empty() && near(doc.objects[0].x, 100.0) && near(doc.walls[0].x, 100.0) && !editor.can_undo(), "and the one before");
		check(!editor.redo().empty() && near(doc.objects[0].x, 300.0) && editor.can_redo(), "redo does it again");

		check(editor.apply({ { "op", "add" }, { "mesh", "meshes/barrel.glb" }, { "x", 1.0 }, { "y", 1.0 }, { "role", "prop" } }).empty()
			&& doc.objects.size() == 4 && doc.objects[3].key == "barrel_01" && doc.objects[3].role == "prop" && !editor.can_redo(),
			"an added object gets a key, and a new operation drops what could be redone");
		check(editor.apply({ { "op", "copy" }, { "object", "traffic_cone_01" }, { "x", 9.0 }, { "y", 9.0 }, { "key", "cone_copy" } }).empty()
			&& doc.objects.back().key == "cone_copy" && doc.objects.back().id == -1, "a copy has its own key and no game id");
		check(editor.apply({ { "op", "delete" }, { "object", "id:7" } }).empty() && doc.find("traffic_cone_01") < 0 && doc.walls.empty(),
			"a deleted object takes its collision with it");

		check(editor.apply({ { "op", "zone" }, { "name", "plaza" }, { "kind", "park" }, { "rect", { 0, 0, 10, 10 } } }).empty()
			&& editor.apply({ { "op", "zoning" } }).empty() && doc.objects[doc.find("barrel_01")].zone == "plaza"
			&& doc.objects[doc.find("building_house_01")].zone.empty(), "zones name places, and objects learn which they stand in");
		check(doc.validate().empty() || doc.validate()[0].find("error") == std::string::npos, "the edited level is valid");

		// the history lasts between runs
		const std::filesystem::path folder = std::filesystem::temp_directory_path() / "pakal_level_editor_test";
		std::filesystem::remove_all(folder);
		std::string error;
		const size_t done = editor.done().size();
		check(editor.save_history((folder / "history").string(), error), "the history is saved");
		LevelEditor again(doc);
		check(again.load_history((folder / "history").string(), error) && again.done().size() == done && !again.undo().empty()
			&& doc.find("barrel_01") >= 0 && doc.objects[doc.find("barrel_01")].zone.empty(), "and read back: it can still undo");

		// split in files per section, and read back whole
		doc.parts = { "objects", "collision" };
		const std::string level = (folder / "level.json").string();
		check(doc.save(level, error) && std::filesystem::exists(folder / "level.objects.json") && std::filesystem::exists(folder / "level.collision.json"),
			"a level is saved split in files per section");
		LevelDocument loaded;
		check(loaded.load(level, error) && loaded.objects.size() == doc.objects.size() && loaded.parts.size() == 2
			&& loaded.find("barrel_01") >= 0 && loaded.zones.size() == 1, "and loads whole, knowing its parts");
		std::filesystem::remove_all(folder);
	}

	std::printf("%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures == 0 ? 0 : 1;
}
