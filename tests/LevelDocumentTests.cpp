///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelDocumentTests.cpp
// Purpose: Round trip of the level document: a version 1 file is upgraded, saved, and reloaded
//          without losing data; unknown sections survive; validation finds broken data. A mission's layer
//          over a base level (LevelLayers) is composed with it.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "LevelDocument.h"
#include "LevelLayers.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
	int g_failures = 0;

	void check(bool condition, const char* what)
	{
		if (!condition)
		{
			++g_failures;
			std::cout << "FAILED: " << what << std::endl;
		}
	}

	Pakal::Json v1_sample()
	{
		return Pakal::Json::parse(R"({
			"format": "pakal-level", "version": 1, "name": "sample", "units_per_meter": 100,
			"spawn": [10.0, 20.0],
			"roles": {"player": "meshes/p.dae"},
			"walls": [{"shape": "rect", "x": 1.0, "y": 2.0, "w": 3.0, "h": 4.0}],
			"enemies": [{"x": 5.0, "y": 6.0, "speed": 150.0}],
			"pickups": [{"x": 7.0, "y": 8.0}],
			"objects": [{"name": "Box", "mesh": "meshes/box.dae", "x": 1, "y": 2, "z": 3,
				"rot": [0, 0, 90], "scale": 1.5}],
			"future_section": {"kept": true}
		})");
	}
}

int main()
{
	std::string error;

	// Upgrade from version 1
	Pakal::LevelDocument doc;
	check(doc.from_json(v1_sample(), error), "version 1 loads");
	check(doc.version == Pakal::LevelDocument::kCurrentVersion, "version 1 is upgraded to the current version");
	check(doc.walls.size() == 1 && doc.walls[0].source == "prop", "v1 walls are marked as prop collision");
	check(doc.hasSpawn && doc.spawn[0] == 10.0 && doc.spawn[1] == 20.0, "spawn is read");
	check(doc.objects.size() == 1 && doc.objects[0].rot[2] == 90.0, "object rotation is read");
	check(doc.extra.contains("future_section"), "unknown top-level sections are kept");

	// Save and reload
	const std::string path = "level_doc_roundtrip.json";
	check(doc.save(path, error), "save succeeds");
	Pakal::LevelDocument again;
	check(again.load(path, error), "saved file reloads");
	check(again.to_json() == doc.to_json(), "reloaded document equals the saved one");
	check(again.extra.contains("future_section") && again.extra["future_section"]["kept"] == true,
		"unknown sections survive a save");
	std::filesystem::remove(path);

	// Validation
	Pakal::LevelDocument broken;
	std::string ignored;
	broken.from_json(Pakal::Json::parse(R"({"format":"pakal-level","version":2,"walls":[{"shape":"rect","w":0,"h":1}]})"), ignored);
	const auto problems = broken.validate();
	bool sawSpawn = false, sawWall = false;
	for (const auto& line : problems)
	{
		sawSpawn = sawSpawn || line.find("no spawn point") != std::string::npos;
		sawWall = sawWall || line.find("positive size") != std::string::npos;
	}
	check(sawSpawn, "validation reports a missing spawn");
	check(sawWall, "validation reports a rect without size");

	// Newer files are refused, not damaged
	Pakal::LevelDocument future;
	check(!future.from_json(Pakal::Json::parse(R"({"format":"pakal-level","version":99})"), ignored),
		"a newer version is refused");

	// A layer over a base: hides a building with its collision, moves a car, adds a barricade, and its rules and light win
	{
		namespace fs = std::filesystem;
		const fs::path root = fs::temp_directory_path() / "pakal_layers_test";
		fs::create_directories(root / "base" / "missions");
		std::ofstream(root / "base" / "level.json") << R"({"format":"pakal-level","version":2,"name":"town",
			"objects":[{"key":"house_1","mesh":"meshes/house.gltf","x":0,"y":0},{"key":"car_1","mesh":"meshes/car.gltf","x":500,"y":0},
			           {"key":"lamp_1","mesh":"../shared/lamp.gltf","x":900,"y":0}],
			"collision":{"segments":[{"layer":"solid","of":"house_1","points":[[0,0],[1,0],[1,1]]},{"layer":"solid","points":[[5,5],[6,5],[6,6]]},
			                         {"layer":"solid","of":"car_1","points":[[600,0],[500,100],[400,0]]}]},
			"lighting":{"sunColor":[1,1,1],"ambient":[0.5,0.5,0.5]},"rules":{"kills":10,"speed":2}})";
		std::ofstream(root / "base" / "missions" / "roadblock.json") << R"({"format":"pakal-level","version":2,"name":"roadblock",
			"extends":"../level.json","hide":["house_1","gone_9"],"adjust":{"car_1":{"x":650,"rot":[0,0,90]}},
			"objects":[{"key":"barricade_1","mesh":"barricade.gltf","x":100,"y":100}],
			"collision":{"segments":[{"layer":"solid","of":"barricade_1","points":[[1,1],[2,1],[2,2]]}]},
			"lighting":{"sunColor":[1,0.6,0.4]},"rules":{"kills":40}})";
		Pakal::LevelDocument layered;
		Pakal::LevelLayering info;
		std::string layerError;
		const bool ok = Pakal::load_level((root / "base" / "missions" / "roadblock.json").string(), layered, layerError, &info);
		check(ok, "a layer over a base loads");
		if (ok)
		{
			check(info.base == "../level.json" && info.hidden == 1 && info.adjusted == 1 && info.added == 1, "the layering is reported");
			check(layered.name == "roadblock" && layered.find("house_1") < 0, "the hidden building is gone");
			const int car = layered.find("car_1");
			check(car >= 0 && layered.objects[car].x == 650.0, "the adjusted car moved");
			check(car >= 0 && layered.objects[car].mesh == "../meshes/car.gltf", "the base's paths are made relative to the layer");
			const int lamp = layered.find("lamp_1");
			check(lamp >= 0 && layered.objects[lamp].mesh == "../../shared/lamp.gltf", "a base path out of its folder too");
			check(layered.find("barricade_1") >= 0 && layered.objects[layered.find("barricade_1")].mesh == "barricade.gltf",
				"the layer's own objects keep their paths");
			const auto& segments = layered.collision["segments"];
			check(segments.size() == 3 && segments[0].value("of", std::string()) != "house_1", "the hidden building's collision goes with it");
			// the car moved 150 east and turned a quarter: its collision turned about its old place and came along
			const auto& carShape = segments[1]["points"];
			check(std::abs(carShape[0][0].get<double>() - 650.0) < 1e-6 && std::abs(carShape[0][1].get<double>() - 100.0) < 1e-6,
				"an adjusted object's collision moves and turns with it");
			check(layered.lighting["sunColor"][1] == 0.6 && layered.lighting["ambient"][0] == 0.5, "the layer's light merges over the base's");
			check(layered.extra["rules"]["kills"] == 40 && layered.extra["rules"]["speed"] == 2, "the layer's rules merge over the base's");
			check(layered.meta["layers"]["base"] == "../level.json", "the composed level says where its base is");
			check(info.missing.size() == 1 && info.missing[0] == "gone_9", "a key the base does not have is reported");
		}
		Pakal::LevelDocument plain;
		check(Pakal::load_level((root / "base" / "level.json").string(), plain, layerError) && plain.objects.size() == 3,
			"a level without a base loads as it is");
		fs::remove_all(root);
	}

	if (g_failures == 0)
	{
		std::cout << "ALL PASSED (0 failures)" << std::endl;
		return 0;
	}
	std::cout << g_failures << " failure(s)" << std::endl;
	return 1;
}
