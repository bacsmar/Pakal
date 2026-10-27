///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelDocument.h
// Purpose: The data of a level (what level.json holds), with no dependency on the engine.
//          Reads and writes the versioned "pakal-level" format, upgrades older files, and checks the
//          data. The game module builds its runtime from a document; the editor edits a document.
//
// Units: positions are in level units (centimeters, Y forward, Z up) as in the original game; use
//        meters() to convert.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <nlohmann/json.hpp>

#include <array>
#include <map>
#include <string>
#include <vector>

namespace Pakal
{
	// Key order of the document is kept, so saved files are stable and readable
	using Json = nlohmann::ordered_json;

	struct LevelWall
	{
		std::string shape = "rect"; // "rect" (w, h) or "circle" (d: diameter)
		double x = 0.0, y = 0.0;
		double w = 0.0, h = 0.0;
		double d = 0.0;
		std::string source; // where it comes from: "prop" (an object's own collision), "world" (level polygon)
		int id = -1;        // game id of the object the collision belongs to, -1 when unknown
	};

	struct LevelEnemy
	{
		double x = 0.0, y = 0.0;
		double speed = 100.0; // units per second
	};

	struct LevelPickup
	{
		double x = 0.0, y = 0.0;
	};

	struct LevelObject
	{
		std::string key;  // readable and unique in the level ("traffic_cone_03"): how tools and agents name it
		std::string name;
		int id = -1;      // the game's object id (scripts show, hide and move objects by it), -1 when unknown
		std::string role; // what it is for ("building", "prop", "light"...), free text the tools filter by
		std::string zone; // the zone it belongs to (LevelZone::name), when it has one
		std::string mesh; // path relative to the level file
		double x = 0.0, y = 0.0, z = 0.0;
		std::array<double, 3> rot = { 0.0, 0.0, 0.0 }; // degrees about X, Y, Z (Z up), applied as Rz*Ry*Rx
		double scale = 1.0;
		// Its place in the level's lightmap atlas: scale, u0, v0 (uv = (u0, v0) + the model's second UV set * scale). Scale 0: none
		std::array<double, 3> lightmap = { 0.0, 0.0, 0.0 };
		bool baked = false; // part of the fixed scene the lightmap was baked with: its shadow on the floor is in it
	};

	// A named rectangle of the level (level units) with a kind ("street", "park", "spawn"...): where things are, for tools
	struct LevelZone
	{
		std::string name;
		std::string kind;
		double x0 = 0.0, y0 = 0.0, x1 = 0.0, y1 = 0.0;
		bool contains(double x, double y) const { return x >= x0 && x <= x1 && y >= y0 && y <= y1; }
	};

	class LevelDocument
	{
	public:
		static constexpr int kCurrentVersion = 2;
		static constexpr const char* kFormat = "pakal-level";

		int version = kCurrentVersion;
		std::string name = "level";
		std::string mode;
		double unitsPerMeter = 100.0;

		bool hasSpawn = false;
		std::array<double, 2> spawn = { 0.0, 0.0 };

		std::map<std::string, std::string> roles; // "player", "enemy", "pickup" -> model path

		std::vector<LevelWall> walls;
		std::vector<LevelEnemy> enemies;
		std::vector<LevelPickup> pickups;
		std::vector<LevelObject> objects;
		std::vector<LevelZone> zones;

		// Sections kept in files of their own next to the level ("objects" -> <level>.objects.json), so a tool or an agent
		// reads only the part it needs. load() fills it from the file's "parts"; save() writes them so.
		std::vector<std::string> parts;

		// Sections the runtime and the editor fill in as the level grows. They are kept as written, so
		// a file with sections this build does not understand is never damaged by loading and saving it.
		Json collision = Json::object(); // { "polys": [...], "props": [...] }
		Json triggers = Json::array();
		Json spawners = Json::array();
		Json waypoints = Json::array();
		Json scene = Json::object();
		Json script = Json::object();
		Json materials = Json::object();
		Json lighting = Json::object();
		Json meta = Json::object();
		Json extra = Json::object(); // any other top-level key

		float meters(double units) const { return static_cast<float>(units / unitsPerMeter); }

		// Reads a file. Older versions are upgraded in memory (version becomes kCurrentVersion).
		bool load(const std::string& path, std::string& error);
		bool from_json(const Json& root, std::string& error);

		bool save(const std::string& path, std::string& error) const;
		Json to_json() const;

		// Problems found in the data, one line each: "error: ..." (the level cannot run) or
		// "warning: ...". With a level directory, the models are checked to exist.
		std::vector<std::string> validate(const std::string& levelDir = std::string()) const;

		// Directory of a level file path, with the trailing separator ("" when it has none)
		static std::string directory_of(const std::string& path);
		// The file of a part of a level file: "dir/level.json", "objects" -> "dir/level.objects.json"
		static std::string part_path(const std::string& path, const std::string& section);

		// Gives every object without a key a readable one, from its name (or model) and a number: "traffic_cone_03".
		// Returns how many it gave.
		int assign_keys();
		// The index of an object by its key, by "#<index>" or by "id:<game id>"; -1 when there is none
		int find(const std::string& reference) const;
		const LevelZone* zone(const std::string& name) const;
	};
}
