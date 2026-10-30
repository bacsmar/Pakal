///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelDocument.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "LevelDocument.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

namespace Pakal
{
	namespace
	{
		const char* const kKnownKeys[] = {
			"format", "version", "name", "mode", "units_per_meter", "spawn", "roles", "walls", "enemies",
			"pickups", "objects", "collision", "triggers", "spawners", "waypoints", "scene", "script",
			"materials", "lighting", "meta", "zones", "parts"
		};

		bool is_known_key(const std::string& key)
		{
			for (const char* known : kKnownKeys)
			{
				if (key == known)
				{
					return true;
				}
			}
			return false;
		}

		// Section stored in the file if it has the expected type, otherwise the default
		Json section(const Json& root, const char* key, const Json& fallback)
		{
			auto it = root.find(key);
			if (it != root.end() && it->type() == fallback.type())
			{
				return *it;
			}
			return fallback;
		}

		bool finite(double v)
		{
			return std::isfinite(v);
		}

		// Writes to a temporary file and renames it: a failed save never leaves half a file behind
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
				if (!file)
				{
					error = "write failed for '" + temporary + "'";
					return false;
				}
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

		// "Traffic Cone" -> "traffic_cone"
		std::string slug(const std::string& text)
		{
			std::string out;
			for (char c : text)
			{
				const unsigned char u = static_cast<unsigned char>(c);
				if (std::isalnum(u)) out += static_cast<char>(std::tolower(u));
				else if (!out.empty() && out.back() != '_') out += '_';
			}
			while (!out.empty() && out.back() == '_') out.pop_back();
			return out.empty() ? std::string("object") : out;
		}
	}

	std::string LevelDocument::part_path(const std::string& path, const std::string& section)
	{
		const std::filesystem::path file(path);
		return (file.parent_path() / (file.stem().string() + "." + section + ".json")).string();
	}

	int LevelDocument::assign_keys()
	{
		std::set<std::string> taken;
		for (const auto& o : objects)
		{
			if (!o.key.empty()) taken.insert(o.key);
		}
		std::map<std::string, int> counts;
		int given = 0;
		for (auto& o : objects)
		{
			if (!o.key.empty()) continue;
			std::string base = o.name;
			if (base.empty()) base = o.prefab;
			if (base.empty()) base = std::filesystem::path(o.mesh).stem().string();
			base = slug(base);
			std::string key;
			do
			{
				char number[16];
				std::snprintf(number, sizeof(number), "_%02d", ++counts[base]);
				key = base + number;
			} while (taken.count(key));
			o.key = key;
			taken.insert(key);
			++given;
		}
		return given;
	}

	int LevelDocument::find(const std::string& reference) const
	{
		if (reference.size() > 1 && reference[0] == '#')
		{
			const int index = std::atoi(reference.c_str() + 1);
			return index >= 0 && index < static_cast<int>(objects.size()) ? index : -1;
		}
		if (reference.rfind("id:", 0) == 0)
		{
			const int id = std::atoi(reference.c_str() + 3);
			for (size_t i = 0; i < objects.size(); ++i)
			{
				if (objects[i].id == id) return static_cast<int>(i);
			}
			return -1;
		}
		for (size_t i = 0; i < objects.size(); ++i)
		{
			if (objects[i].key == reference) return static_cast<int>(i);
		}
		return -1;
	}

	const LevelZone* LevelDocument::zone(const std::string& name) const
	{
		for (const auto& z : zones)
		{
			if (z.name == name) return &z;
		}
		return nullptr;
	}

	std::string LevelDocument::directory_of(const std::string& path)
	{
		const size_t cut = path.find_last_of("/\\");
		return cut == std::string::npos ? std::string() : path.substr(0, cut + 1);
	}

	namespace
	{
		LevelDocument::FileReader& file_reader()
		{
			static LevelDocument::FileReader reader;
			return reader;
		}
	}

	void LevelDocument::set_file_reader(FileReader reader)
	{
		file_reader() = std::move(reader);
	}

	bool LevelDocument::read_file(const std::string& path, std::string& text)
	{
		if (file_reader()) return file_reader()(path, text);
		std::ifstream file(path, std::ios::binary);
		if (!file) return false;
		text.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
		return true;
	}

	bool LevelDocument::load(const std::string& path, std::string& error)
	{
		std::string text;
		if (!read_file(path, text))
		{
			error = "cannot open '" + path + "'";
			return false;
		}
		Json root = Json::parse(text, nullptr, false);
		if (root.is_discarded())
		{
			error = "'" + path + "' is not valid JSON";
			return false;
		}
		// sections kept in files of their own: read back into the document
		std::vector<std::string> partNames;
		if (root.contains("parts") && root["parts"].is_object())
		{
			const std::filesystem::path dir = std::filesystem::path(path).parent_path();
			const Json names = root["parts"];   // a copy: adding the sections to the root moves its keys
			for (auto it = names.begin(); it != names.end(); ++it)
			{
				const std::string partFile = (dir / it.value().get<std::string>()).generic_string();
				std::string partText;
				const bool found = read_file(partFile, partText);
				Json part = found ? Json::parse(partText, nullptr, false) : Json();
				if (!found || part.is_discarded())
				{
					error = "cannot read the part '" + it.key() + "' of '" + path + "' ('" + partFile + "')";
					return false;
				}
				root[it.key()] = std::move(part);
				partNames.push_back(it.key());
			}
		}
		if (!from_json(root, error)) return false;
		parts = partNames;
		return true;
	}

	bool LevelDocument::from_json(const Json& root, std::string& error)
	{
		if (!root.is_object() || root.value("format", std::string()) != kFormat)
		{
			error = "not a pakal-level document";
			return false;
		}

		const int fileVersion = root.value("version", 1);
		if (fileVersion > kCurrentVersion)
		{
			error = "level version " + std::to_string(fileVersion) + " is newer than this build supports ("
				+ std::to_string(kCurrentVersion) + ")";
			return false;
		}

		LevelDocument doc;
		doc.name = root.value("name", std::string("level"));
		doc.mode = root.value("mode", std::string());
		doc.unitsPerMeter = root.value("units_per_meter", 100.0);

		const Json spawnJson = root.value("spawn", Json::array());
		if (spawnJson.is_array() && spawnJson.size() == 2 && spawnJson[0].is_number() && spawnJson[1].is_number())
		{
			doc.hasSpawn = true;
			doc.spawn = { spawnJson[0].get<double>(), spawnJson[1].get<double>() };
		}

		if (root.contains("roles") && root["roles"].is_object())
		{
			for (auto it = root["roles"].begin(); it != root["roles"].end(); ++it)
			{
				if (it.value().is_string())
				{
					doc.roles[it.key()] = it.value().get<std::string>();
				}
			}
		}

		// Files of version 1 did not say where a wall came from; the converter wrote only prop collision
		const bool v1 = fileVersion < 2;
		for (const auto& wall : root.value("walls", Json::array()))
		{
			LevelWall w;
			w.shape = wall.value("shape", std::string("rect"));
			w.x = wall.value("x", 0.0);
			w.y = wall.value("y", 0.0);
			w.w = wall.value("w", 0.0);
			w.h = wall.value("h", 0.0);
			w.angle = wall.value("angle", 0.0);
			w.d = wall.value("d", 0.0);
			w.source = wall.value("source", std::string(v1 ? "prop" : ""));
			w.id = wall.value("id", -1);
			doc.walls.push_back(w);
		}
		for (const auto& enemy : root.value("enemies", Json::array()))
		{
			doc.enemies.push_back({ enemy.value("x", 0.0), enemy.value("y", 0.0), enemy.value("speed", 100.0) });
		}
		for (const auto& pickup : root.value("pickups", Json::array()))
		{
			doc.pickups.push_back({ pickup.value("x", 0.0), pickup.value("y", 0.0) });
		}
		for (const auto& object : root.value("objects", Json::array()))
		{
			LevelObject o;
			o.key = object.value("key", std::string());
			o.name = object.value("name", std::string());
			o.id = object.value("id", -1);
			o.role = object.value("role", std::string());
			o.zone = object.value("zone", std::string());
			o.mesh = object.value("mesh", std::string());
			o.prefab = object.value("prefab", std::string());
			if (object.contains("overrides") && object["overrides"].is_object()) o.overrides = object["overrides"];
			o.x = object.value("x", 0.0);
			o.y = object.value("y", 0.0);
			o.z = object.value("z", 0.0);
			o.scale = object.value("scale", 1.0);
			const Json rot = object.value("rot", Json::array());
			for (size_t i = 0; i < 3 && i < rot.size(); ++i)
			{
				if (rot[i].is_number())
				{
					o.rot[i] = rot[i].get<double>();
				}
			}
			const Json lightmap = object.value("lightmap", Json::array());
			for (size_t i = 0; i < 3 && i < lightmap.size(); ++i)
			{
				if (lightmap[i].is_number())
				{
					o.lightmap[i] = lightmap[i].get<double>();
				}
			}
			o.baked = object.value("baked", false);
			if (object.contains("paint") && object["paint"].is_object()) o.paint = object["paint"];
			const Json basis = object.value("basis", Json::array());
			if (basis.is_array() && basis.size() == 9)
			{
				o.has_basis = true;
				for (size_t i = 0; i < 9; ++i) o.basis[i] = basis[i].is_number() ? basis[i].get<double>() : 0.0;
			}
			doc.objects.push_back(o);
		}
		for (const auto& zone : root.value("zones", Json::array()))
		{
			LevelZone z;
			z.name = zone.value("name", std::string());
			z.kind = zone.value("kind", std::string());
			const Json rect = zone.value("rect", Json::array());
			if (rect.is_array() && rect.size() == 4)
			{
				z.x0 = std::min(rect[0].get<double>(), rect[2].get<double>());
				z.y0 = std::min(rect[1].get<double>(), rect[3].get<double>());
				z.x1 = std::max(rect[0].get<double>(), rect[2].get<double>());
				z.y1 = std::max(rect[1].get<double>(), rect[3].get<double>());
			}
			doc.zones.push_back(z);
		}

		doc.collision = section(root, "collision", Json::object());
		doc.triggers = section(root, "triggers", Json::array());
		doc.spawners = section(root, "spawners", Json::array());
		doc.waypoints = section(root, "waypoints", Json::array());
		doc.scene = section(root, "scene", Json::object());
		doc.script = section(root, "script", Json::object());
		doc.materials = section(root, "materials", Json::object());
		doc.lighting = section(root, "lighting", Json::object());
		doc.meta = section(root, "meta", Json::object());

		for (auto it = root.begin(); it != root.end(); ++it)
		{
			if (!is_known_key(it.key()))
			{
				doc.extra[it.key()] = it.value();
			}
		}

		doc.version = kCurrentVersion;
		*this = std::move(doc);
		return true;
	}

	Json LevelDocument::to_json() const
	{
		Json root = Json::object();
		root["format"] = kFormat;
		root["version"] = version;
		root["name"] = name;
		if (!mode.empty())
		{
			root["mode"] = mode;
		}
		root["units_per_meter"] = unitsPerMeter;
		if (hasSpawn)
		{
			root["spawn"] = Json::array({ spawn[0], spawn[1] });
		}

		Json rolesJson = Json::object();
		for (const auto& role : roles)
		{
			rolesJson[role.first] = role.second;
		}
		root["roles"] = rolesJson;

		Json wallsJson = Json::array();
		for (const auto& w : walls)
		{
			Json item = Json::object();
			item["shape"] = w.shape;
			item["x"] = w.x;
			item["y"] = w.y;
			if (w.shape == "circle")
			{
				item["d"] = w.d;
			}
			else
			{
				item["w"] = w.w;
				item["h"] = w.h;
				if (w.angle != 0.0) item["angle"] = w.angle;
			}
			if (!w.source.empty())
			{
				item["source"] = w.source;
			}
			if (w.id >= 0)
			{
				item["id"] = w.id;
			}
			wallsJson.push_back(item);
		}
		root["walls"] = wallsJson;

		Json enemiesJson = Json::array();
		for (const auto& e : enemies)
		{
			enemiesJson.push_back(Json{ { "x", e.x }, { "y", e.y }, { "speed", e.speed } });
		}
		root["enemies"] = enemiesJson;

		Json pickupsJson = Json::array();
		for (const auto& p : pickups)
		{
			pickupsJson.push_back(Json{ { "x", p.x }, { "y", p.y } });
		}
		root["pickups"] = pickupsJson;

		Json objectsJson = Json::array();
		for (const auto& o : objects)
		{
			Json item = Json::object();
			if (!o.key.empty()) item["key"] = o.key;
			item["name"] = o.name;
			if (o.id != -1) item["id"] = o.id;   // the converter's negative ids (-11...) mean something to the game too
			if (!o.role.empty()) item["role"] = o.role;
			if (!o.zone.empty()) item["zone"] = o.zone;
			if (!o.prefab.empty()) item["prefab"] = o.prefab;
			if (!o.mesh.empty() || o.prefab.empty()) item["mesh"] = o.mesh;
			if (!o.overrides.empty()) item["overrides"] = o.overrides;
			item["x"] = o.x;
			item["y"] = o.y;
			item["z"] = o.z;
			item["rot"] = Json::array({ o.rot[0], o.rot[1], o.rot[2] });
			item["scale"] = o.scale;
			if (o.lightmap[0] > 0.0) item["lightmap"] = Json::array({ o.lightmap[0], o.lightmap[1], o.lightmap[2] });
			if (o.baked) item["baked"] = true;
			if (o.paint.is_object()) item["paint"] = o.paint;
			if (o.has_basis)
			{
				Json basis = Json::array();
				for (double value : o.basis) basis.push_back(value);
				item["basis"] = basis;
			}
			objectsJson.push_back(item);
		}
		root["objects"] = objectsJson;
		if (!zones.empty())
		{
			Json zonesJson = Json::array();
			for (const auto& z : zones)
			{
				zonesJson.push_back(Json{ { "name", z.name }, { "kind", z.kind }, { "rect", Json::array({ z.x0, z.y0, z.x1, z.y1 }) } });
			}
			root["zones"] = zonesJson;
		}

		root["collision"] = collision;
		root["triggers"] = triggers;
		root["spawners"] = spawners;
		root["waypoints"] = waypoints;
		root["scene"] = scene;
		root["script"] = script;
		root["materials"] = materials;
		root["lighting"] = lighting;
		root["meta"] = meta;
		for (auto it = extra.begin(); it != extra.end(); ++it)
		{
			root[it.key()] = it.value();
		}
		return root;
	}

	bool LevelDocument::save(const std::string& path, std::string& error) const
	{
		Json root = to_json();
		// each part in its file first, then the level that names them
		if (!parts.empty())
		{
			Json names = Json::object();
			for (const auto& section : parts)
			{
				if (!root.contains(section)) continue;
				const std::string file = part_path(path, section);
				if (!write_json(file, root[section], error)) return false;
				names[section] = std::filesystem::path(file).filename().string();
				root.erase(section);
			}
			root["parts"] = names;
		}
		return write_json(path, root, error);
	}

	std::vector<std::string> LevelDocument::validate(const std::string& levelDir) const
	{
		std::vector<std::string> problems;
		auto error = [&](const std::string& text) { problems.push_back("error: " + text); };
		auto warning = [&](const std::string& text) { problems.push_back("warning: " + text); };

		if (unitsPerMeter <= 0.0 || !finite(unitsPerMeter))
		{
			error("units_per_meter must be positive");
		}
		if (!hasSpawn)
		{
			error("the level has no spawn point");
		}
		else if (!finite(spawn[0]) || !finite(spawn[1]))
		{
			error("the spawn point is not a finite position");
		}

		for (const char* role : { "player", "enemy", "pickup" })
		{
			if (!roles.count(role))
			{
				warning(std::string("no model for the role '") + role + "'");
			}
		}

		for (size_t i = 0; i < walls.size(); ++i)
		{
			const LevelWall& w = walls[i];
			const std::string where = "wall " + std::to_string(i);
			if (w.shape != "rect" && w.shape != "circle")
			{
				error(where + " has the unknown shape '" + w.shape + "'");
			}
			else if (w.shape == "rect" && (w.w <= 0.0 || w.h <= 0.0))
			{
				error(where + " is a rect without a positive size");
			}
			else if (w.shape == "circle" && w.d <= 0.0)
			{
				error(where + " is a circle without a positive diameter");
			}
			if (!finite(w.x) || !finite(w.y))
			{
				error(where + " is not at a finite position");
			}
		}

		std::set<std::string> keys;
		for (size_t i = 0; i < objects.size(); ++i)
		{
			const LevelObject& o = objects[i];
			const std::string where = "object " + std::to_string(i) + " ('" + (o.key.empty() ? o.name : o.key) + "')";
			if (!o.key.empty() && !keys.insert(o.key).second)
			{
				error(where + " has a key another object has");
			}
			if (!o.zone.empty() && !zone(o.zone))
			{
				warning(where + " is in the unknown zone '" + o.zone + "'");
			}
			if (o.mesh.empty() && o.prefab.empty())
			{
				error(where + " has no model and no prefab");
			}
			else if (!o.mesh.empty() && !levelDir.empty() && !std::filesystem::exists(std::filesystem::path(levelDir) / o.mesh))
			{
				warning(where + " uses the missing model '" + o.mesh + "'");
			}
			if (!finite(o.x) || !finite(o.y) || !finite(o.z) || !finite(o.scale) || o.scale == 0.0)
			{
				error(where + " has an invalid position or scale");
			}
		}

		if (!levelDir.empty())
		{
			for (const auto& role : roles)
			{
				if (!std::filesystem::exists(std::filesystem::path(levelDir) / role.second))
				{
					warning("the model of the role '" + role.first + "' is missing: '" + role.second + "'");
				}
			}
		}
		return problems;
	}
}
