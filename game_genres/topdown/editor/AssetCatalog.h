///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: AssetCatalog.h
// Purpose: The models a level can be made of, for the editor's asset browser: each one's name, pack,
//          file, roles and tags, read from what the tools already keep, without the engine:
//          - the level builder's catalog (tools/levelbuilder/catalog.json: models by pack, with their size,
//            roles and tags), found from the game's folder up;
//          - the manifests of packs extracted under Assets (<root>/installed.json: {"<pack>": {"models":
//            {"<model>": ...}}}, each model at <root>/<pack>/models/<model>.gltf, with its colliders in
//            <model>.colliders.json when the pack has them);
//          - what editor_catalog.json (next to the game) adds: {"catalogs": [...], "manifests": [...],
//            "folders": [{"folder", "pack", "roles"}], "roles": [{"match": "<regex>", "roles": [...]}]}.
//          A model placed from the browser becomes a prefab (prefab_for): its mesh, its role and tags, and
//          its colliders when it has them.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "LevelDocument.h"

#include <array>
#include <string>
#include <vector>

namespace Pakal::TopDown
{
	struct AssetEntry
	{
		std::string id;          // the model's name (its file's, without the extension)
		std::string pack;
		std::string file;        // the model's file, from the game's folder
		std::string colliders;   // its colliders' file (<model>.colliders.json), "" when it has none
		std::vector<std::string> roles, tags;
		std::array<float, 3> size{};   // meters across (east, north, up) when the catalog says; 0 unknown
		std::string search;      // id, pack, roles and tags in lower case: what the browser's search box matches
	};

	class AssetCatalog
	{
	public:
		static constexpr const char* kSettingsFile = "editor_catalog.json";

		// Reads every source it finds from 'gameFolder' (the working folder by default). Replaces what it had.
		void load(const std::string& gameFolder = ".");
		const std::vector<AssetEntry>& entries() const { return m_entries; }
		// What was read, one line each ("12 models from ..."), and what could not be
		const std::vector<std::string>& sources() const { return m_sources; }
		// The packs and the roles the entries have, sorted
		std::vector<std::string> packs() const;
		std::vector<std::string> roles() const;
		// The entries whose name, pack, roles or tags hold every word of 'text' (case aside), of the pack and role given
		// ("" any), by name
		std::vector<size_t> filter(const std::string& text, const std::string& pack, const std::string& role) const;
		int find(const std::string& pack, const std::string& id) const;

		// The prefab of a model, as written into 'folder' (from the game's folder): its mesh relative to it, its role (its
		// first tag, else its first role), its tags, where it comes from ("catalog": "<pack>/<model>"), and its colliders
		// ("collision": {"source": "prefab", "shapes": [...]}, in the model's axes) when it has them
		static Pakal::Json prefab_for(const AssetEntry& entry, const std::string& folder);
		// The name its prefab takes ("<model>", letters, digits, '_' and '-' only); "<model>_<pack>" when that name is
		// taken by a prefab from elsewhere ('takenBy' says which "catalog" a prefab of that name has, "" none)
		static std::string prefab_name(const AssetEntry& entry, const std::string& takenBy);

	private:
		void add_catalog(const std::string& path);
		void add_manifest(const std::string& path);
		void add_folder(const std::string& folder, const std::string& pack, const std::vector<std::string>& roles);
		void add(AssetEntry entry);

		std::vector<AssetEntry> m_entries;
		std::vector<std::string> m_sources;
		std::vector<std::pair<std::string, std::vector<std::string>>> m_roleRules;   // a name's pattern, the roles it gives
	};
}
