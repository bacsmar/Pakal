///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelDocumentTests.cpp
// Purpose: Round trip of the level document: a version 1 file is upgraded, saved, and reloaded
//          without losing data; unknown sections survive; validation finds broken data.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "LevelDocument.h"

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

	if (g_failures == 0)
	{
		std::cout << "ALL PASSED (0 failures)" << std::endl;
		return 0;
	}
	std::cout << g_failures << " failure(s)" << std::endl;
	return 1;
}
