///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PrefabLibrary.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "PrefabLibrary.h"

#include <algorithm>
#include <fstream>

namespace Pakal
{
	namespace
	{
		const Json& null_json()
		{
			static const Json none;
			return none;
		}

		std::string with_separator(std::string folder)
		{
			if (!folder.empty() && folder.back() != '/' && folder.back() != '\\') folder += '/';
			return folder;
		}

		// "dir/../prefabs/props/Barrel.glb" seen from the level's folder: "prefabs/props/Barrel.glb"
		std::string relative_to(const std::string& file, const std::string& folder)
		{
			const std::filesystem::path normal = std::filesystem::path(file).lexically_normal();
			if (folder.empty()) return normal.generic_string();
			const std::filesystem::path relative = normal.lexically_relative(std::filesystem::path(folder).lexically_normal());
			return relative.empty() ? normal.generic_string() : relative.generic_string();
		}

		bool write_json(const std::string& path, const Json& json, std::string& error)
		{
			const std::string temporary = path + ".tmp";
			{
				std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
				if (!file)
				{
					error = "cannot write '" + temporary + "'";
					return false;
				}
				file << json.dump(1) << "\n";
			}
			std::error_code code;
			std::filesystem::rename(temporary, path, code);
			if (code)
			{
				std::filesystem::remove(path, code);
				std::filesystem::rename(temporary, path, code);
				if (code)
				{
					error = "cannot replace '" + path + "': " + code.message();
					return false;
				}
			}
			return true;
		}
	}

	void PrefabLibrary::open(const std::string& levelPath, const LevelDocument& document)
	{
		const std::string levelDir = LevelDocument::directory_of(levelPath);
		std::string folder = "prefabs";
		if (document.meta.contains("prefabs") && document.meta["prefabs"].is_string()) folder = document.meta["prefabs"].get<std::string>();
		set_folder(levelDir + folder, levelDir);
	}

	void PrefabLibrary::set_folder(const std::string& folder, const std::string& levelDir)
	{
		m_folder = with_separator(folder);
		m_levelDir = with_separator(levelDir);
		m_cache.clear();
	}

	std::vector<std::string> PrefabLibrary::names() const
	{
		std::vector<std::string> found;
		std::error_code code;
		const std::string extension = kExtension;
		for (const auto& entry : std::filesystem::directory_iterator(m_folder.empty() ? "." : m_folder, code))
		{
			const std::string file = entry.path().filename().string();
			if (entry.is_regular_file(code) && file.size() > extension.size() && file.compare(file.size() - extension.size(), extension.size(), extension) == 0)
			{
				found.push_back(file.substr(0, file.size() - extension.size()));
			}
		}
		std::sort(found.begin(), found.end());
		return found;
	}

	std::string PrefabLibrary::path_of(const std::string& name) const
	{
		return m_folder + name + kExtension;
	}

	bool PrefabLibrary::exists(const std::string& name) const
	{
		std::error_code code;
		return !name.empty() && std::filesystem::is_regular_file(path_of(name), code);
	}

	const PrefabLibrary::Entry* PrefabLibrary::load(const std::string& name)
	{
		auto cached = m_cache.find(name);
		if (cached != m_cache.end()) return &cached->second;
		const std::string path = path_of(name);
		std::ifstream in(path, std::ios::binary);
		if (!in) return nullptr;
		Entry entry;
		entry.json = Json::parse(in, nullptr, false);
		if (entry.json.is_discarded() || !entry.json.is_object()) return nullptr;
		std::error_code code;
		entry.time = std::filesystem::last_write_time(path, code);
		return &(m_cache[name] = std::move(entry));
	}

	const Json& PrefabLibrary::raw(const std::string& name)
	{
		const Entry* entry = load(name);
		return entry ? entry->json : null_json();
	}

	Json PrefabLibrary::prefab(const std::string& name, std::string& error)
	{
		// the chain of bases, the prefab last
		std::vector<const Json*> chain;
		std::string current = name;
		while (!current.empty())
		{
			if (static_cast<int>(chain.size()) >= kMaxDepth)
			{
				error = "the prefab '" + name + "' has bases " + std::to_string(kMaxDepth) + " deep or in a loop";
				return Json();
			}
			const Entry* entry = load(current);
			if (!entry)
			{
				error = "no prefab '" + current + "' (" + path_of(current) + ")";
				return Json();
			}
			chain.push_back(&entry->json);
			current = entry->json.value("base", std::string());
		}
		Json merged = Json::object();
		for (auto it = chain.rbegin(); it != chain.rend(); ++it)
		{
			Json layer = **it;
			// each layer's mesh is relative to the folder of the prefabs (all in one folder)
			if (layer.contains("mesh") && layer["mesh"].is_string())
			{
				layer["mesh"] = relative_to(m_folder + layer["mesh"].get<std::string>(), m_levelDir);
			}
			merged.merge_patch(layer);
		}
		merged.erase("base");
		return merged;
	}

	Json PrefabLibrary::resolve(const LevelObject& object, std::string& error)
	{
		Json result = Json::object();
		if (!object.prefab.empty())
		{
			result = prefab(object.prefab, error);
			if (!error.empty()) return Json();
		}
		// an object without a prefab can still carry components of its own in its overrides
		if (object.overrides.is_object()) result.merge_patch(object.overrides);
		if (!object.mesh.empty()) result["mesh"] = object.mesh;
		if (!object.role.empty()) result["role"] = object.role;
		if (!result.contains("mesh") || !result["mesh"].is_string()) result["mesh"] = std::string();
		if (!result.contains("role") || !result["role"].is_string()) result["role"] = std::string();
		if (!result.contains("components") || !result["components"].is_object()) result["components"] = Json::object();
		if (!result.contains("collision")) result["collision"] = Json();
		return result;
	}

	std::string PrefabLibrary::level_to_prefab_path(const std::string& levelRelative) const
	{
		return relative_to(m_levelDir + levelRelative, m_folder);
	}

	bool PrefabLibrary::save(const std::string& name, const Json& prefab, std::string& error)
	{
		if (name.empty() || name.find_first_of("/\\:") != std::string::npos)
		{
			error = "the prefab name '" + name + "' is not a file name";
			return false;
		}
		std::error_code code;
		std::filesystem::create_directories(m_folder, code);
		m_cache.erase(name);
		return write_json(path_of(name), prefab, error);
	}

	std::vector<std::string> PrefabLibrary::refresh()
	{
		std::vector<std::string> changed;
		for (auto it = m_cache.begin(); it != m_cache.end();)
		{
			std::error_code code;
			const auto time = std::filesystem::last_write_time(path_of(it->first), code);
			if (code || time != it->second.time)
			{
				changed.push_back(it->first);
				it = m_cache.erase(it);
			}
			else
			{
				++it;
			}
		}
		return changed;
	}

	void PrefabLibrary::set_override(Json& overrides, const std::string& path, const Json& value)
	{
		if (!overrides.is_object()) overrides = Json::object();
		overrides[Json::json_pointer(path)] = value;
	}

	void PrefabLibrary::revert_override(Json& overrides, const std::string& path)
	{
		if (path.empty() || path == "/")
		{
			overrides = Json::object();
			return;
		}
		Json::json_pointer pointer(path);
		if (!overrides.contains(pointer)) return;
		while (!pointer.empty())
		{
			const std::string last = pointer.back();
			const Json::json_pointer parent = pointer.parent_pointer();
			Json& holder = parent.empty() ? overrides : overrides[parent];
			holder.erase(last);
			// a parent left empty goes too
			if (parent.empty() || !holder.empty()) break;
			pointer = parent;
		}
	}

	Json PrefabLibrary::diff(const Json& original, const Json& changed)
	{
		if (!original.is_object() || !changed.is_object())
		{
			return original == changed ? Json::object() : changed;
		}
		Json patch = Json::object();
		for (auto it = changed.begin(); it != changed.end(); ++it)
		{
			auto was = original.find(it.key());
			if (was == original.end())
			{
				patch[it.key()] = it.value();
			}
			else if (was->is_object() && it.value().is_object())
			{
				Json inner = diff(*was, it.value());
				if (!inner.empty()) patch[it.key()] = std::move(inner);
			}
			else if (*was != it.value())
			{
				patch[it.key()] = it.value();
			}
		}
		for (auto it = original.begin(); it != original.end(); ++it)
		{
			if (!changed.contains(it.key())) patch[it.key()] = nullptr;
		}
		return patch;
	}
}
