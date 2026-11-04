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
//
//          Besides its own folder, a level can name shared ones ("meta": {"prefabLibraries": ["../../prefabs"]},
//          relative to the level): a prefab not in its own folder is looked for in them, in order. A level
//          without the key has only its own folder.
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

		// The prefabs of a level: the folder its "meta.prefabs" names (relative to the level), "prefabs" by default, and the
		// shared ones its "meta.prefabLibraries" names
		void open(const std::string& levelPath, const LevelDocument& document);
		void set_folder(const std::string& folder, const std::string& levelDir);
		const std::string& folder() const { return m_folder; }
		// Shared folders looked in after the level's own, in order (full paths or relative to the game's folder)
		void set_libraries(const std::vector<std::string>& folders);
		const std::vector<std::string>& libraries() const { return m_libraries; }
		static constexpr const char* kLibrariesKey = "prefabLibraries";

		// Every prefab's name in the folder and the libraries, sorted (a name in two of them once: the first one's)
		std::vector<std::string> names() const;
		bool exists(const std::string& name) const;
		// Its file: in the level's folder, else in the first library that has it, else (a new one) in the level's folder
		std::string path_of(const std::string& name) const;
		// The folder its file is in (with a trailing separator), as path_of
		std::string folder_of(const std::string& name) const;

		// The prefab as its file holds it (null when there is none or it is not valid JSON)
		const Json& raw(const std::string& name);
		// The prefab with its bases merged in, its mesh made relative to the level ("" error when fine)
		Json prefab(const std::string& name, std::string& error);
		// What an object is: its prefab with its overrides merged in, or (without a prefab) its own mesh and role.
		// Always has "mesh" (relative to the level), "role", "components" (an object) and "collision" (maybe null).
		Json resolve(const LevelObject& object, std::string& error);

		// A path relative to the level, made relative to the prefabs' folder (what a prefab's "mesh" holds)
		std::string level_to_prefab_path(const std::string& levelRelative) const;
		// A path relative to the game's folder, made relative to a folder (what a prefab written there holds as "mesh")
		static std::string relative_path(const std::string& file, const std::string& folder);

		// Writes a prefab's file where it is (path_of) and forgets what was read of it
		bool save(const std::string& name, const Json& prefab, std::string& error);
		// Writes a new prefab's file into a folder of its own choosing (the level's or a library)
		bool save_in(const std::string& folder, const std::string& name, const Json& prefab, std::string& error);

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
			std::string folder;   // where its file is
		};
		const Entry* load(const std::string& name);

		std::string m_folder;     // the prefabs' folder, with a trailing separator
		std::vector<std::string> m_libraries;   // shared folders after it, each with a trailing separator
		std::string m_levelDir;   // the level's folder, with a trailing separator
		std::map<std::string, Entry> m_cache;
	};
}
