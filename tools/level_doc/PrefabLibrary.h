///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PrefabLibrary.h
// Purpose: The prefabs of a level: reusable objects, one file each (<folder>/<Name>.prefab.json), with
//          their model, collision and components. A level's object made from one names it ("prefab":
//          "Barrel") and keeps only what it changes ("overrides"); editing the prefab changes every
//          object made from it. A prefab can be a variant of another ("base": "Barrel").
//
//          A prefab file:
//            { "mesh": "props/Barrel.glb", "role": "prop", "tags": ["breakable"],
//              "collision": { "shape": "box", "size": [0.8, 0.8] },
//              "components": { "Destructible": { "health": 40 }, "Explosive": { "radius": 3 } } }
//          The mesh is relative to the prefab's file. Overrides are a JSON merge patch (RFC 7386) on the
//          prefab: objects merge, anything else replaces, null removes.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "LevelDocument.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace Pakal
{
	class PrefabLibrary
	{
	public:
		static constexpr const char* kExtension = ".prefab.json";
		static constexpr int kMaxDepth = 8;   // how many bases deep a variant can go

		// The prefabs of a level: the folder its "meta.prefabs" names (relative to the level), "prefabs" by default
		void open(const std::string& levelPath, const LevelDocument& document);
		void set_folder(const std::string& folder, const std::string& levelDir);
		const std::string& folder() const { return m_folder; }

		// Every prefab's name in the folder, sorted
		std::vector<std::string> names() const;
		bool exists(const std::string& name) const;
		std::string path_of(const std::string& name) const;

		// The prefab as its file holds it (null when there is none or it is not valid JSON)
		const Json& raw(const std::string& name);
		// The prefab with its bases merged in, its mesh made relative to the level ("" error when fine)
		Json prefab(const std::string& name, std::string& error);
		// What an object is: its prefab with its overrides merged in, or (without a prefab) its own mesh and role.
		// Always has "mesh" (relative to the level), "role", "components" (an object) and "collision" (maybe null).
		Json resolve(const LevelObject& object, std::string& error);

		// A path relative to the level, made relative to the prefabs' folder (what a prefab's "mesh" holds)
		std::string level_to_prefab_path(const std::string& levelRelative) const;

		// Writes a prefab's file (and forgets what was read of it)
		bool save(const std::string& name, const Json& prefab, std::string& error);

		// Forgets the prefabs whose files changed since they were read; returns their names
		std::vector<std::string> refresh();

		// Overrides helpers. A path is a JSON pointer into the prefab ("/components/Destructible/health").
		static void set_override(Json& overrides, const std::string& path, const Json& value);
		// Removes one override (and the objects it leaves empty), or every one with an empty path
		static void revert_override(Json& overrides, const std::string& path);
		// What 'changed' holds that 'original' does not: the smallest merge patch from one to the other
		static Json diff(const Json& original, const Json& changed);

	private:
		struct Entry
		{
			Json json;
			std::filesystem::file_time_type time{};
		};
		const Entry* load(const std::string& name);

		std::string m_folder;     // the prefabs' folder, with a trailing separator
		std::string m_levelDir;   // the level's folder, with a trailing separator
		std::map<std::string, Entry> m_cache;
	};
}
