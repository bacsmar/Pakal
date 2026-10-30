///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelLayerEditing.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "LevelLayerEditing.h"

#include "PrefabLibrary.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace Pakal
{
	namespace fs = std::filesystem;

	namespace
	{
		bool read_json(const std::string& path, Json& out)
		{
			std::ifstream in(path, std::ios::binary);
			if (!in) return false;
			out = Json::parse(in, nullptr, false);
			return !out.is_discarded();
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
			fs::rename(temporary, path, code);
			if (code)
			{
				fs::remove(path, code);
				fs::rename(temporary, path, code);
				if (code)
				{
					error = "cannot replace '" + path + "': " + code.message();
					return false;
				}
			}
			return true;
		}
	}

	bool LevelLayerEditing::open(const std::string& path, LevelDocument& composed, std::string& error)
	{
		*this = LevelLayerEditing();
		if (!load_level(path, composed, error, &m_layering)) return false;
		if (m_layering.base.empty()) return true;   // a whole level: edited as it is

		m_layered = true;
		if (!read_json(path, m_raw) || !m_raw.is_object())
		{
			error = "cannot read '" + path + "'";
			return false;
		}
		const std::string layerDir = LevelDocument::directory_of(path);
		m_basePath = (fs::path(layerDir.empty() ? "." : layerDir) / m_layering.base).lexically_normal().generic_string();
		LevelDocument base;
		if (!load_level(m_basePath, base, error))
		{
			error = "the base of '" + path + "': " + error;
			return false;
		}
		base.parts.clear();
		// the base as the layer sees it: its paths from the layer's folder, nothing hidden or adjusted
		LevelLayering unused;
		const Json seen = compose_layer(base.to_json(), LevelDocument::directory_of(m_basePath), Json::object(), layerDir, unused);
		LevelDocument reference;
		if (!reference.from_json(seen, error)) return false;
		size_t keyless = 0;
		const Json objects = reference.to_json()["objects"];
		for (const auto& object : objects)
		{
			const std::string key = object.value("key", std::string());
			if (key.empty())
			{
				++keyless;
				continue;
			}
			m_order.push_back(key);
			m_reference[key] = object;
		}
		if (keyless > 0)
		{
			error = std::to_string(keyless) + " objects of the base '" + m_basePath + "' have no key: a layer names them by key (pakal-level <base> keys)";
			*this = LevelLayerEditing();
			return false;
		}
		for (const auto& zone : reference.zones) m_baseZones.insert(zone.name);
		const Json all = reference.to_json();
		if (all.contains("spawn")) m_baseSpawn = all["spawn"];
		if (all.contains("camera") && all["camera"].is_object()) m_baseCamera = all["camera"];
		return true;
	}

	std::vector<std::string> LevelLayerEditing::hidden(const LevelDocument& current) const
	{
		std::set<std::string> present;
		for (const auto& object : current.objects) present.insert(object.key);
		std::vector<std::string> gone;
		for (const auto& key : m_order)
		{
			if (!present.count(key)) gone.push_back(key);
		}
		return gone;
	}

	Json LevelLayerEditing::base_object(const std::string& key) const
	{
		auto found = m_reference.find(key);
		return found != m_reference.end() ? found->second : Json();
	}

	int LevelLayerEditing::base_index(const std::string& key, const LevelDocument& current) const
	{
		// the base's objects come first in a composed level, in the base's order: after the shown ones before it
		std::set<std::string> present;
		for (const auto& object : current.objects) present.insert(object.key);
		int index = 0;
		for (const auto& k : m_order)
		{
			if (k == key) return index;
			if (present.count(k)) ++index;
		}
		return -1;
	}

	Json LevelLayerEditing::layer_json(const LevelDocument& current) const
	{
		Json out = m_raw;
		const Json all = current.to_json();

		// the ids the layer hides by "id:" stay as they are, and so do the objects they hide
		std::set<long long> hiddenIds;
		Json hide = Json::array();
		for (const auto& entry : m_raw.value("hide", Json::array()))
		{
			if (!entry.is_string() || entry.get<std::string>().rfind("id:", 0) != 0) continue;
			hide.push_back(entry);
			try { hiddenIds.insert(std::stoll(entry.get<std::string>().substr(3))); }
			catch (...) {}
		}

		Json added = Json::array();
		Json adjust = Json::object();
		std::set<std::string> present;
		for (const auto& object : all.value("objects", Json::array()))
		{
			const std::string key = object.value("key", std::string());
			present.insert(key);
			auto base = m_reference.find(key);
			if (base == m_reference.end())
			{
				added.push_back(object);
				continue;
			}
			const Json patch = PrefabLibrary::diff(base->second, object);
			if (!patch.empty()) adjust[key] = patch;
		}
		for (const auto& key : m_order)
		{
			if (present.count(key)) continue;
			const Json& object = m_reference.at(key);
			if (object.contains("id") && object["id"].is_number_integer() && hiddenIds.count(object["id"].get<long long>())) continue;
			hide.push_back(key);
		}

		Json zones = Json::array();
		for (const auto& zone : all.value("zones", Json::array()))
		{
			if (!m_baseZones.count(zone.value("name", std::string()))) zones.push_back(zone);
		}

		out["objects"] = added;
		auto set_or_erase = [&out](const char* name, const Json& value)
		{
			if (value.empty()) out.erase(name);
			else out[name] = value;
		};
		set_or_erase("hide", hide);
		set_or_erase("adjust", adjust);
		set_or_erase("zones", zones);
		if (all.contains("spawn") && all["spawn"] != m_baseSpawn) out["spawn"] = all["spawn"];

		// the camera paths: one the layer changed is written whole (it replaces the base's), one it removed as null
		const Json camera = all.contains("camera") && all["camera"].is_object() ? all["camera"] : Json::object();
		Json paths = Json::object();
		for (auto it = camera.begin(); it != camera.end(); ++it)
		{
			if (m_baseCamera.contains(it.key()) && m_baseCamera[it.key()] == it.value()) continue;
			paths[it.key()] = it.value();
			// composing merges a path over the base's key by key: what the base's has and this one does not is taken out
			const Json& base = m_baseCamera.contains(it.key()) ? m_baseCamera[it.key()] : Json();
			if (base.is_object() && it.value().is_object())
			{
				for (auto field = base.begin(); field != base.end(); ++field)
				{
					if (!it.value().contains(field.key())) paths[it.key()][field.key()] = nullptr;
				}
			}
		}
		for (auto it = m_baseCamera.begin(); it != m_baseCamera.end(); ++it)
		{
			if (!camera.contains(it.key())) paths[it.key()] = nullptr;
		}
		set_or_erase("camera", paths);
		return out;
	}

	bool LevelLayerEditing::save(const std::string& path, const LevelDocument& current, std::string& error) const
	{
		return write_json(path, layer_json(current), error);
	}

	bool LevelLayerEditing::create(const std::string& layerPath, const std::string& basePath, std::string& error)
	{
		const fs::path layer = fs::path(layerPath).lexically_normal();
		const fs::path base = fs::path(basePath).lexically_normal();
		std::error_code code;
		if (fs::exists(layer, code))
		{
			error = "'" + layerPath + "' exists";
			return false;
		}
		if (layer == base)
		{
			error = "a layer cannot be its own base";
			return false;
		}
		if (!layer.parent_path().empty()) fs::create_directories(layer.parent_path(), code);
		fs::path relative = base.lexically_relative(layer.parent_path().empty() ? fs::path(".") : layer.parent_path());
		if (relative.empty()) relative = fs::absolute(base);
		const std::string name = layer.parent_path().filename().string();
		const Json json = { { "format", LevelDocument::kFormat }, { "version", LevelDocument::kCurrentVersion },
			{ "name", name.empty() ? std::string("layer") : name }, { "extends", relative.generic_string() }, { "objects", Json::array() } };
		return write_json(layer.string(), json, error);
	}
}
