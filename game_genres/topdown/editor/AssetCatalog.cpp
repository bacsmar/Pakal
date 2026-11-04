///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: AssetCatalog.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/editor/AssetCatalog.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>

namespace Pakal::TopDown
{
	using Pakal::Json;

	namespace
	{
		Json read_json(const std::string& path)
		{
			std::ifstream in(path, std::ios::binary);
			if (!in) return Json();
			Json json = Json::parse(in, nullptr, false);
			return json.is_discarded() ? Json() : json;
		}

		std::string lower(std::string text)
		{
			for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			return text;
		}

		std::string generic(const std::filesystem::path& path)
		{
			return path.lexically_normal().generic_string();
		}

		std::vector<std::string> strings(const Json& list)
		{
			std::vector<std::string> out;
			for (const auto& item : list.is_array() ? list : Json::array())
			{
				if (item.is_string()) out.push_back(item.get<std::string>());
			}
			return out;
		}

		// A model named the way many packs name theirs ("SM_Bld_House_01", "SM_Prop_Crate_02"): its kind from the name's
		// second part. A name that does not follow it gets none.
		std::vector<std::string> roles_from_name(const std::string& id)
		{
			static const std::map<std::string, std::string> kinds = {
				{ "bld", "building" }, { "building", "building" }, { "prop", "prop" }, { "props", "prop" }, { "veh", "vehicle" },
				{ "vehicle", "vehicle" }, { "env", "environment" }, { "gen", "generic" }, { "wep", "weapon" }, { "chr", "character" },
				{ "sign", "sign" }, { "item", "prop" }, { "fx", "effect" }, { "tree", "vegetation" }, { "plant", "vegetation" },
				{ "rock", "rock" }, { "road", "street" }, { "street", "street" }, { "wall", "wall" }, { "fence", "fence" },
			};
			std::vector<std::string> parts;
			std::stringstream stream(id);
			for (std::string part; std::getline(stream, part, '_');) parts.push_back(lower(part));
			if (parts.size() < 2) return {};
			const auto kind = kinds.find(parts[1]);
			if (kind == kinds.end()) return {};
			std::vector<std::string> roles = { kind->second };
			// the third part says more of what it is ("SM_Env_Tree_01": a tree among the scenery)
			if (parts.size() > 2)
			{
				const auto more = kinds.find(parts[2]);
				if (more != kinds.end() && more->second != kind->second) roles.push_back(more->second);
			}
			return roles;
		}
	}

	void AssetCatalog::load(const std::string& gameFolder)
	{
		m_entries.clear();
		m_sources.clear();
		m_roleRules.clear();
		const std::filesystem::path game(gameFolder.empty() ? "." : gameFolder);
		std::error_code code;

		// what the game's own settings add (read first: its rules for roles apply to every source)
		Json settings = read_json((game / kSettingsFile).string());
		if (!settings.is_object()) settings = Json::object();   // none (or not an object): value() would throw
		for (const auto& rule : settings.value("roles", Json::array()))
		{
			if (rule.is_object() && rule.contains("match") && rule["match"].is_string()) m_roleRules.push_back({ rule["match"].get<std::string>(), strings(rule.value("roles", Json::array())) });
		}

		// the level builder's catalog, from the game's folder up
		std::set<std::string> catalogs;
		for (std::filesystem::path at = std::filesystem::absolute(game, code); !at.empty();)
		{
			const std::filesystem::path candidate = at / "tools" / "levelbuilder" / "catalog.json";
			if (std::filesystem::is_regular_file(candidate, code))
			{
				catalogs.insert(generic(candidate));
				break;
			}
			if (at == at.parent_path()) break;
			at = at.parent_path();
		}
		for (const auto& path : strings(settings.value("catalogs", Json::array()))) catalogs.insert(generic(game / path));
		for (const auto& path : catalogs) add_catalog(path);

		// the packs extracted under Assets, each with its manifest
		std::set<std::string> manifests;
		for (const auto& entry : std::filesystem::directory_iterator(game / "Assets", code))
		{
			const std::filesystem::path manifest = entry.path() / "installed.json";
			if (entry.is_directory(code) && std::filesystem::is_regular_file(manifest, code)) manifests.insert(generic(manifest));
		}
		for (const auto& path : strings(settings.value("manifests", Json::array()))) manifests.insert(generic(game / path));
		for (const auto& path : manifests) add_manifest(path);

		for (const auto& folder : settings.value("folders", Json::array()))
		{
			if (!folder.is_object() || !folder.contains("folder")) continue;
			const std::string path = folder["folder"].get<std::string>();
			add_folder(generic(game / path), folder.value("pack", std::filesystem::path(path).filename().string()), strings(folder.value("roles", Json::array())));
		}
		if (m_entries.empty()) m_sources.push_back("no models: no catalog, pack manifest or editor_catalog.json found");
		std::sort(m_entries.begin(), m_entries.end(), [](const AssetEntry& a, const AssetEntry& b) { return a.pack != b.pack ? a.pack < b.pack : a.id < b.id; });
	}

	void AssetCatalog::add(AssetEntry entry)
	{
		for (const auto& [pattern, roles] : m_roleRules)
		{
			try
			{
				if (!std::regex_search(entry.id, std::regex(pattern))) continue;
			}
			catch (const std::regex_error&)
			{
				continue;
			}
			for (const auto& role : roles)
			{
				if (std::find(entry.roles.begin(), entry.roles.end(), role) == entry.roles.end()) entry.roles.push_back(role);
			}
		}
		std::error_code code;
		// from the game's folder (the working one) when it can be: what a level's paths are made from
		if (std::filesystem::path(entry.file).is_absolute()) entry.file = generic(std::filesystem::path(entry.file).lexically_proximate(std::filesystem::current_path(code)));
		const std::filesystem::path colliders = std::filesystem::path(entry.file).replace_extension().string() + ".colliders.json";
		if (entry.colliders.empty() && std::filesystem::is_regular_file(colliders, code)) entry.colliders = generic(colliders);
		entry.search = lower(entry.id + " " + entry.pack);
		for (const auto& role : entry.roles) entry.search += " " + lower(role);
		for (const auto& tag : entry.tags) entry.search += " " + lower(tag);
		m_entries.push_back(std::move(entry));
	}

	void AssetCatalog::add_catalog(const std::string& path)
	{
		const Json catalog = read_json(path);
		const Json models = catalog.value("models", Json::object());
		if (!models.is_object() || models.empty())
		{
			m_sources.push_back("not a catalog: " + path);
			return;
		}
		// its paths are from the repository's folder, three up from the catalog (tools/levelbuilder/catalog.json)
		const std::filesystem::path root = std::filesystem::path(path).parent_path().parent_path().parent_path();
		size_t count = 0;
		std::error_code code;
		for (auto it = models.begin(); it != models.end(); ++it)
		{
			const Json& model = it.value();
			if (!model.is_object() || !model.contains("path")) continue;
			AssetEntry entry;
			entry.id = it.key();
			entry.pack = model.value("pack", std::string("catalog"));
			entry.file = generic(root / model["path"].get<std::string>());
			if (!std::filesystem::is_regular_file(entry.file, code)) continue;   // a pack not installed here
			entry.roles = strings(model.value("roles", Json::array()));
			entry.tags = strings(model.value("tags", Json::array()));
			const Json size = model.value("size", Json::array());
			for (size_t i = 0; i < 3 && i < size.size(); ++i) entry.size[i] = size[i].is_number() ? size[i].get<float>() : 0.0f;
			add(std::move(entry));
			++count;
		}
		m_sources.push_back(std::to_string(count) + " models from " + path);
	}

	void AssetCatalog::add_manifest(const std::string& path)
	{
		const Json manifest = read_json(path);
		if (!manifest.is_object())
		{
			m_sources.push_back("not a manifest: " + path);
			return;
		}
		const std::filesystem::path root = std::filesystem::path(path).parent_path();
		size_t count = 0;
		std::error_code code;
		for (auto pack = manifest.begin(); pack != manifest.end(); ++pack)
		{
			const Json models = pack.value().is_object() ? pack.value().value("models", Json::object()) : Json::object();
			for (auto it = models.begin(); it != models.end(); ++it)
			{
				AssetEntry entry;
				entry.id = it.key();
				entry.pack = pack.key();
				const std::filesystem::path folder = root / pack.key() / "models";
				entry.file = generic(folder / (entry.id + ".gltf"));
				if (!std::filesystem::is_regular_file(entry.file, code))
				{
					entry.file = generic(folder / (entry.id + ".glb"));
					if (!std::filesystem::is_regular_file(entry.file, code)) continue;
				}
				entry.roles = roles_from_name(entry.id);
				add(std::move(entry));
				++count;
			}
		}
		m_sources.push_back(std::to_string(count) + " models from " + path);
	}

	void AssetCatalog::add_folder(const std::string& folder, const std::string& pack, const std::vector<std::string>& roles)
	{
		size_t count = 0;
		std::error_code code;
		for (auto it = std::filesystem::recursive_directory_iterator(folder, std::filesystem::directory_options::skip_permission_denied, code);
			it != std::filesystem::recursive_directory_iterator(); it.increment(code))
		{
			if (code) break;
			const std::string extension = lower(it->path().extension().string());
			if (!it->is_regular_file(code) || (extension != ".gltf" && extension != ".glb")) continue;
			AssetEntry entry;
			entry.id = it->path().stem().string();
			entry.pack = pack;
			entry.file = generic(it->path());
			entry.roles = roles.empty() ? roles_from_name(entry.id) : roles;
			add(std::move(entry));
			++count;
		}
		m_sources.push_back(std::to_string(count) + " models from " + folder);
	}

	std::vector<std::string> AssetCatalog::packs() const
	{
		std::set<std::string> found;
		for (const auto& entry : m_entries) found.insert(entry.pack);
		return { found.begin(), found.end() };
	}

	std::vector<std::string> AssetCatalog::roles() const
	{
		std::set<std::string> found;
		for (const auto& entry : m_entries) found.insert(entry.roles.begin(), entry.roles.end());
		return { found.begin(), found.end() };
	}

	std::vector<size_t> AssetCatalog::filter(const std::string& text, const std::string& pack, const std::string& role) const
	{
		std::vector<std::string> words;
		std::stringstream stream(lower(text));
		for (std::string word; stream >> word;) words.push_back(word);
		std::vector<size_t> found;
		for (size_t i = 0; i < m_entries.size(); ++i)
		{
			const AssetEntry& entry = m_entries[i];
			if (!pack.empty() && entry.pack != pack) continue;
			if (!role.empty() && std::find(entry.roles.begin(), entry.roles.end(), role) == entry.roles.end()) continue;
			bool all = true;
			for (const auto& word : words) all = all && entry.search.find(word) != std::string::npos;
			if (all) found.push_back(i);
		}
		return found;
	}

	int AssetCatalog::find(const std::string& pack, const std::string& id) const
	{
		for (size_t i = 0; i < m_entries.size(); ++i)
		{
			if (m_entries[i].id == id && (pack.empty() || m_entries[i].pack == pack)) return static_cast<int>(i);
		}
		return -1;
	}

	Json AssetCatalog::prefab_for(const AssetEntry& entry, const std::string& folder)
	{
		Json prefab = Json::object();
		// both made full first: the catalog's paths and the folder may start from different places
		const std::filesystem::path file = std::filesystem::absolute(entry.file).lexically_normal();
		const std::filesystem::path relative = file.lexically_relative(std::filesystem::absolute(folder).lexically_normal());
		prefab["mesh"] = (relative.empty() ? file : relative).generic_string();
		const std::string role = !entry.tags.empty() ? entry.tags.front() : (!entry.roles.empty() ? entry.roles.front() : std::string("prop"));
		prefab["role"] = role;
		if (!entry.tags.empty()) prefab["tags"] = entry.tags;
		prefab["catalog"] = entry.pack + "/" + entry.id;
		if (!entry.colliders.empty())
		{
			const Json colliders = read_json(entry.colliders);
			if (colliders.is_object() && colliders.contains("shapes")) prefab["collision"] = colliders;
		}
		return prefab;
	}

	std::string AssetCatalog::prefab_name(const AssetEntry& entry, const std::string& takenBy)
	{
		auto clean = [](const std::string& text)
		{
			std::string out;
			for (char c : text) out += (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') ? c : '_';
			return out;
		};
		const std::string name = clean(entry.id);
		if (takenBy.empty() || takenBy == entry.pack + "/" + entry.id) return name;
		return name + "_" + clean(entry.pack);
	}
}
