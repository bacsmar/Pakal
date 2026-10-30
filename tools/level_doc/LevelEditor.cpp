///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelEditor.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "LevelEditor.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace Pakal
{
	namespace
	{
		bool number(const Json& op, const char* field, double& value)
		{
			auto it = op.find(field);
			if (it == op.end() || !it->is_number()) return false;
			value = it->get<double>();
			return true;
		}

		Json read_file(const std::string& path)
		{
			std::ifstream in(path, std::ios::binary);
			return in ? Json::parse(in, nullptr, false) : Json();
		}

		bool write_file(const std::string& path, const Json& json)
		{
			std::ofstream out(path, std::ios::binary | std::ios::trunc);
			out << json.dump();
			return static_cast<bool>(out);
		}

		// The states are whole documents, restored keeping which sections live in files of their own
		void restore(LevelDocument& document, const Json& state)
		{
			const std::vector<std::string> parts = document.parts;
			std::string error;
			document.from_json(state, error);
			document.parts = parts;
		}
	}

	const std::vector<std::string>& LevelEditor::help()
	{
		static const std::vector<std::string> lines = {
			"move    object, x, y [, z]  or  object, dx, dy [, dz]   (meters; its collision moves with it)",
			"turn    object, yaw  or  object, by                      (degrees about the vertical axis)",
			"scale   object, scale",
			"add     mesh, x, y [, z, yaw, scale, name, key, role, zone]",
			"copy    object, x, y [, key]                             (a copy without the game id)",
			"delete  object                                            (and its collision)",
			"set     object, field, value                              (field: key, name, role, zone, mesh, baked, id)",
			"zone    name, kind, rect: [x0, y0, x1, y1]                (adds or changes a zone; meters)",
			"unzone  name",
			"spawn   x, y                                              (where the player starts)",
			"camera  [path,] data                                      (a camera path as JSON, \"intro\" by default; data null removes it)",
			"place   object [, x, y, z, rot: [x, y, z], scale, basis: [9]]  (meters, degrees; what a gizmo does; rot or scale drop a basis)",
			"instantiate  prefab, x, y [, z, yaw, scale, key, zone, role]  (an object made from a prefab)",
			"override     object, path, value                         (path: a JSON pointer into its prefab, \"/components/Destructible/health\")",
			"revert       object [, path]                             (one override, or all of them)",
			"apply-to-prefab  object                                  (its overrides go into its prefab's file; every object made from it changes)",
			"make-prefab  object, name                                (a prefab file from the object; the object is made from it)",
			"insert       data [, at]                                 (an object as JSON, as the level file writes it; at: its index)",
			"keys                                                      (a readable key for every object without one)",
			"zoning  [all]                                             (each object's zone from where it stands)",
		};
		return lines;
	}

	std::string LevelEditor::apply(const Json& operation)
	{
		Json before = m_document.to_json();
		const std::string error = run(operation);
		if (!error.empty())
		{
			restore(m_document, before);
			return error;
		}
		m_undo.push_back(std::move(before));
		if (m_undo.size() > kHistory) m_undo.erase(m_undo.begin());
		m_redo.clear();
		m_undoneOps.clear();
		remember(operation);
		return std::string();
	}

	void LevelEditor::remember(const Json& operation)
	{
		m_done.push_back(operation);
		if (m_done.size() > kHistory) m_done.erase(m_done.begin());
	}

	std::string LevelEditor::undo()
	{
		if (m_undo.empty()) return std::string();
		m_redo.push_back(m_document.to_json());
		restore(m_document, m_undo.back());
		m_undo.pop_back();
		Json operation = m_done.empty() ? Json::object() : m_done.back();
		if (!m_done.empty()) m_done.pop_back();
		m_undoneOps.push_back(operation);
		return operation.dump();
	}

	std::string LevelEditor::redo()
	{
		if (m_redo.empty()) return std::string();
		m_undo.push_back(m_document.to_json());
		restore(m_document, m_redo.back());
		m_redo.pop_back();
		Json operation = m_undoneOps.empty() ? Json::object() : m_undoneOps.back();
		if (!m_undoneOps.empty()) m_undoneOps.pop_back();
		remember(operation);
		return operation.dump();
	}

	std::string LevelEditor::run(const Json& op)
	{
		LevelDocument& doc = m_document;
		const std::string name = op.value("op", std::string());
		const double units = doc.unitsPerMeter;
		auto object = [&](int& index) -> std::string
		{
			const std::string reference = op.value("object", std::string());
			index = doc.find(reference);
			return index < 0 ? "no object '" + reference + "'" : std::string();
		};
		auto key_free = [&](const std::string& key, int except) -> bool
		{
			const int found = doc.find(key);
			return key.empty() || found < 0 || found == except;
		};
		double x = 0.0, y = 0.0, z = 0.0, value = 0.0;
		int index = -1;

		if (name == "move")
		{
			if (std::string error = object(index); !error.empty()) return error;
			LevelObject& o = doc.objects[static_cast<size_t>(index)];
			double dx = 0.0, dy = 0.0, dz = 0.0;
			if (number(op, "x", x) && number(op, "y", y))
			{
				dx = x * units - o.x;
				dy = y * units - o.y;
				if (number(op, "z", z)) dz = z * units - o.z;
			}
			else if (number(op, "dx", dx) || number(op, "dy", dy) || number(op, "dz", dz))
			{
				number(op, "dx", dx);
				number(op, "dy", dy);
				number(op, "dz", dz);
				dx *= units;
				dy *= units;
				dz *= units;
			}
			else
			{
				return "move needs x and y, or dx and dy";
			}
			o.x += dx;
			o.y += dy;
			o.z += dz;
			// its own collision goes with it
			if (o.id >= 0)
			{
				for (auto& wall : doc.walls)
				{
					if (wall.id == o.id)
					{
						wall.x += dx;
						wall.y += dy;
					}
				}
			}
			return std::string();
		}
		if (name == "turn")
		{
			if (std::string error = object(index); !error.empty()) return error;
			LevelObject& o = doc.objects[static_cast<size_t>(index)];
			const double before = o.rot[2];
			if (number(op, "yaw", value)) o.rot[2] = value;
			else if (number(op, "by", value)) o.rot[2] += value;
			else return "turn needs yaw or by";
			// its own collision turns with it, about the object
			if (o.id >= 0 && o.rot[2] != before)
			{
				const double turn = (o.rot[2] - before) * 3.14159265358979 / 180.0;
				const double c = std::cos(turn), s = std::sin(turn);
				for (auto& wall : doc.walls)
				{
					if (wall.id != o.id) continue;
					const double rx = wall.x - o.x, ry = wall.y - o.y;
					wall.x = o.x + rx * c - ry * s;
					wall.y = o.y + rx * s + ry * c;
					if (wall.shape == "rect") wall.angle += o.rot[2] - before;
				}
			}
			return std::string();
		}
		if (name == "scale")
		{
			if (std::string error = object(index); !error.empty()) return error;
			if (!number(op, "scale", value) || value <= 0.0) return "scale needs a positive scale";
			doc.objects[static_cast<size_t>(index)].scale = value;
			return std::string();
		}
		if (name == "add" || name == "copy")
		{
			LevelObject o;
			if (name == "copy")
			{
				if (std::string error = object(index); !error.empty()) return error;
				o = doc.objects[static_cast<size_t>(index)];
				o.id = -1;
				o.key.clear();
			}
			else
			{
				o.mesh = op.value("mesh", std::string());
				if (o.mesh.empty()) return "add needs a mesh";
				o.name = op.value("name", std::filesystem::path(o.mesh).stem().string());
				o.role = op.value("role", std::string());
				o.zone = op.value("zone", std::string());
				if (number(op, "yaw", value)) o.rot[2] = value;
				if (number(op, "scale", value))
				{
					if (value <= 0.0) return "the scale must be positive";
					o.scale = value;
				}
				if (number(op, "z", z)) o.z = z * units;
			}
			if (!number(op, "x", x) || !number(op, "y", y)) return name + " needs x and y";
			o.x = x * units;
			o.y = y * units;
			o.key = op.value("key", std::string());
			if (!key_free(o.key, -1)) return "the key '" + o.key + "' is taken";
			doc.objects.push_back(o);
			if (doc.objects.back().key.empty()) doc.assign_keys();
			return std::string();
		}
		if (name == "delete")
		{
			if (std::string error = object(index); !error.empty()) return error;
			const int id = doc.objects[static_cast<size_t>(index)].id;
			doc.objects.erase(doc.objects.begin() + index);
			if (id >= 0) doc.walls.erase(std::remove_if(doc.walls.begin(), doc.walls.end(), [id](const LevelWall& w) { return w.id == id; }), doc.walls.end());
			return std::string();
		}
		if (name == "set")
		{
			if (std::string error = object(index); !error.empty()) return error;
			LevelObject& o = doc.objects[static_cast<size_t>(index)];
			const std::string field = op.value("field", std::string());
			const Json given = op.value("value", Json());
			auto text = [&]() { return given.is_string() ? given.get<std::string>() : given.dump(); };
			if (field == "key")
			{
				if (text().empty() || !key_free(text(), index)) return "the key '" + text() + "' is empty or taken";
				o.key = text();
			}
			else if (field == "name") o.name = text();
			else if (field == "role") o.role = text();
			else if (field == "zone")
			{
				if (!text().empty() && !doc.zone(text())) return "no zone '" + text() + "'";
				o.zone = text();
			}
			else if (field == "mesh") o.mesh = text();
			else if (field == "prefab")
			{
				if (!text().empty() && m_prefabs && !m_prefabs->exists(text())) return "no prefab '" + text() + "'";
				o.prefab = text();
			}
			else if (field == "baked") o.baked = given.is_boolean() ? given.get<bool>() : text() == "true";
			else if (field == "id") o.id = given.is_number() ? given.get<int>() : std::atoi(text().c_str());
			else return "set cannot change '" + field + "'";
			return std::string();
		}
		if (name == "zone")
		{
			LevelZone zone;
			zone.name = op.value("name", std::string());
			zone.kind = op.value("kind", std::string());
			const Json rect = op.value("rect", Json::array());
			if (zone.name.empty() || !rect.is_array() || rect.size() != 4) return "zone needs a name and rect: [x0, y0, x1, y1]";
			for (const auto& v : rect)
			{
				if (!v.is_number()) return "the rect must be four numbers";
			}
			zone.x0 = std::min(rect[0].get<double>(), rect[2].get<double>()) * units;
			zone.y0 = std::min(rect[1].get<double>(), rect[3].get<double>()) * units;
			zone.x1 = std::max(rect[0].get<double>(), rect[2].get<double>()) * units;
			zone.y1 = std::max(rect[1].get<double>(), rect[3].get<double>()) * units;
			auto found = std::find_if(doc.zones.begin(), doc.zones.end(), [&](const LevelZone& z) { return z.name == zone.name; });
			if (found != doc.zones.end())
			{
				if (zone.kind.empty()) zone.kind = found->kind;
				*found = zone;
			}
			else
			{
				doc.zones.push_back(zone);
			}
			return std::string();
		}
		if (name == "unzone")
		{
			const std::string zoneName = op.value("name", std::string());
			auto found = std::find_if(doc.zones.begin(), doc.zones.end(), [&](const LevelZone& z) { return z.name == zoneName; });
			if (found == doc.zones.end()) return "no zone '" + zoneName + "'";
			doc.zones.erase(found);
			for (auto& o : doc.objects)
			{
				if (o.zone == zoneName) o.zone.clear();
			}
			return std::string();
		}
		if (name == "spawn")
		{
			if (!number(op, "x", x) || !number(op, "y", y)) return "spawn needs x and y";
			doc.hasSpawn = true;
			doc.spawn = { x * units, y * units };
			return std::string();
		}
		if (name == "camera")
		{
			const std::string path = op.value("path", std::string("intro"));
			if (path.empty()) return "camera needs a path's name";
			if (!op.contains("data")) return "camera needs data (null removes the path)";
			const Json& data = op["data"];
			Json& camera = doc.extra["camera"];
			if (!camera.is_object()) camera = Json::object();
			if (data.is_null())
			{
				camera.erase(path);
				if (camera.empty()) doc.extra.erase("camera");
				return std::string();
			}
			if (!data.is_object() || !data.contains("shots") || !data["shots"].is_array()) return "a camera path is an object with \"shots\"";
			for (const auto& shot : data["shots"])
			{
				auto point = [&shot](const char* name) { return shot.contains(name) && shot[name].is_array() && shot[name].size() == 3; };
				if (!point("eye") || !point("look")) return "each shot has \"eye\" and \"look\" as [x, y, z]";
			}
			camera[path] = data;
			return std::string();
		}
		if (name == "place")
		{
			if (std::string error = object(index); !error.empty()) return error;
			LevelObject& o = doc.objects[static_cast<size_t>(index)];
			double dx = 0.0, dy = 0.0;
			if (number(op, "x", x)) { dx = x * units - o.x; o.x = x * units; }
			if (number(op, "y", y)) { dy = y * units - o.y; o.y = y * units; }
			if (number(op, "z", z)) o.z = z * units;
			const Json rot = op.value("rot", Json());
			if (rot.is_array())
			{
				if (rot.size() != 3) return "rot must be three angles";
				for (size_t i = 0; i < 3; ++i)
				{
					if (!rot[i].is_number()) return "rot must be three angles";
					o.rot[i] = rot[i].get<double>();
				}
			}
			if (number(op, "scale", value))
			{
				if (value <= 0.0) return "the scale must be positive";
				o.scale = value;
			}
			// a rotation or a scale given replaces the model's own matrix; a basis given is the matrix
			if (rot.is_array() || op.contains("scale")) o.has_basis = false;
			const Json basis = op.value("basis", Json());
			if (basis.is_array())
			{
				if (basis.size() != 9) return "basis must be nine numbers";
				for (size_t i = 0; i < 9; ++i)
				{
					if (!basis[i].is_number()) return "basis must be nine numbers";
					o.basis[i] = basis[i].get<double>();
				}
				o.has_basis = true;
			}
			if (o.id >= 0 && (dx != 0.0 || dy != 0.0))
			{
				for (auto& wall : doc.walls)
				{
					if (wall.id == o.id) { wall.x += dx; wall.y += dy; }
				}
			}
			return std::string();
		}
		if (name == "instantiate")
		{
			LevelObject o;
			o.prefab = op.value("prefab", std::string());
			if (o.prefab.empty()) return "instantiate needs a prefab";
			if (m_prefabs && !m_prefabs->exists(o.prefab)) return "no prefab '" + o.prefab + "'";
			if (!number(op, "x", x) || !number(op, "y", y)) return "instantiate needs x and y";
			o.name = op.value("name", o.prefab);
			o.role = op.value("role", std::string());
			o.zone = op.value("zone", std::string());
			if (!o.zone.empty() && !doc.zone(o.zone)) return "no zone '" + o.zone + "'";
			o.x = x * units;
			o.y = y * units;
			if (number(op, "z", z)) o.z = z * units;
			if (number(op, "yaw", value)) o.rot[2] = value;
			if (number(op, "scale", value))
			{
				if (value <= 0.0) return "the scale must be positive";
				o.scale = value;
			}
			o.key = op.value("key", std::string());
			if (!key_free(o.key, -1)) return "the key '" + o.key + "' is taken";
			doc.objects.push_back(o);
			if (doc.objects.back().key.empty()) doc.assign_keys();
			return std::string();
		}
		if (name == "override" || name == "revert")
		{
			if (std::string error = object(index); !error.empty()) return error;
			LevelObject& o = doc.objects[static_cast<size_t>(index)];
			const std::string path = op.value("path", std::string());
			if (name == "override" && (path.empty() || path[0] != '/')) return "override needs a path that starts with '/'";
			if (name == "override" && !op.contains("value")) return "override needs a value";
			try
			{
				if (name == "override") PrefabLibrary::set_override(o.overrides, path, op["value"]);
				else PrefabLibrary::revert_override(o.overrides, path);
			}
			catch (const Json::exception& e)
			{
				return std::string("the path '") + path + "' does not fit: " + e.what();
			}
			return std::string();
		}
		if (name == "apply-to-prefab" || name == "make-prefab")
		{
			if (!m_prefabs) return name + " needs the level's prefabs";
			if (std::string error = object(index); !error.empty()) return error;
			LevelObject& o = doc.objects[static_cast<size_t>(index)];
			if (name == "apply-to-prefab")
			{
				if (o.prefab.empty()) return "the object is not made from a prefab";
				if (o.overrides.contains("mesh")) return "a changed model goes into the prefab by hand (its path is the level's)";
				Json prefab = m_prefabs->raw(o.prefab);
				if (prefab.is_null()) return "no prefab '" + o.prefab + "'";
				prefab.merge_patch(o.overrides);
				std::string error;
				if (!m_prefabs->save(o.prefab, prefab, error)) return error;
				o.overrides = Json::object();
				return std::string();
			}
			const std::string prefabName = op.value("name", std::string());
			if (prefabName.empty()) return "make-prefab needs a name";
			if (m_prefabs->exists(prefabName)) return "the prefab '" + prefabName + "' exists";
			std::string error;
			Json prefab = m_prefabs->resolve(o, error);
			if (!error.empty()) return error;
			if (!o.prefab.empty()) prefab["base"] = o.prefab;   // a variant of the one it was made from
			if (prefab["mesh"].get<std::string>().empty()) prefab.erase("mesh");
			else prefab["mesh"] = m_prefabs->level_to_prefab_path(prefab["mesh"].get<std::string>());
			if (prefab["collision"].is_null()) prefab.erase("collision");
			if (prefab["components"].empty()) prefab.erase("components");
			if (prefab["role"].get<std::string>().empty()) prefab.erase("role");
			if (!o.prefab.empty())
			{
				// a variant holds only what differs from its base
				Json base = m_prefabs->prefab(o.prefab, error);
				if (!error.empty()) return error;
				if (base.contains("mesh") && base["mesh"].is_string()) base["mesh"] = m_prefabs->level_to_prefab_path(base["mesh"].get<std::string>());
				Json variant = PrefabLibrary::diff(base, prefab);
				variant.erase("base");
				prefab = Json{ { "base", o.prefab } };
				for (auto it = variant.begin(); it != variant.end(); ++it) prefab[it.key()] = it.value();   // nulls kept: they remove
			}
			if (!m_prefabs->save(prefabName, prefab, error)) return error;
			o.prefab = prefabName;
			o.mesh.clear();
			o.role.clear();
			o.overrides = Json::object();
			return std::string();
		}
		if (name == "insert")
		{
			const Json data = op.value("data", Json());
			if (!data.is_object()) return "insert needs data: an object as the level file writes it";
			// read through the document's own reader: one object, in a document of one
			LevelDocument one;
			std::string error;
			if (!one.from_json(Json{ { "format", LevelDocument::kFormat }, { "version", LevelDocument::kCurrentVersion }, { "units_per_meter", doc.unitsPerMeter },
				{ "objects", Json::array({ data }) } }, error) || one.objects.size() != 1) return "insert: " + error;
			LevelObject o = one.objects.front();
			if (!key_free(o.key, -1)) return "the key '" + o.key + "' is taken";
			const int at = op.value("at", -1);
			if (at >= 0 && at <= static_cast<int>(doc.objects.size())) doc.objects.insert(doc.objects.begin() + at, o);
			else doc.objects.push_back(o);
			if (o.key.empty()) doc.assign_keys();
			return std::string();
		}
		if (name == "keys")
		{
			doc.assign_keys();
			return std::string();
		}
		if (name == "zoning")
		{
			const bool all = op.value("all", false);
			for (auto& o : doc.objects)
			{
				if (!all && !o.zone.empty()) continue;
				o.zone.clear();
				for (const auto& zone : doc.zones)
				{
					if (zone.contains(o.x, o.y))
					{
						o.zone = zone.name;
						break;
					}
				}
			}
			return std::string();
		}
		return "unknown operation '" + name + "'";
	}

	// The folder: index.json (the operations and the files of the states) and one file per state, written once
	bool LevelEditor::save_history(const std::string& folder, std::string& error) const
	{
		std::error_code code;
		std::filesystem::create_directories(folder, code);
		if (code)
		{
			error = "cannot make '" + folder + "'";
			return false;
		}
		Json index = { { "undo", Json::array() }, { "redo", Json::array() }, { "done", m_done }, { "undone", m_undoneOps } };
		for (size_t i = 0; i < m_undo.size(); ++i)
		{
			const std::string file = "undo_" + std::to_string(i) + ".json";
			if (!write_file((std::filesystem::path(folder) / file).string(), m_undo[i])) { error = "cannot write the history"; return false; }
			index["undo"].push_back(file);
		}
		for (size_t i = 0; i < m_redo.size(); ++i)
		{
			const std::string file = "redo_" + std::to_string(i) + ".json";
			if (!write_file((std::filesystem::path(folder) / file).string(), m_redo[i])) { error = "cannot write the history"; return false; }
			index["redo"].push_back(file);
		}
		// states of an older, longer history go
		for (const auto& entry : std::filesystem::directory_iterator(folder, code))
		{
			const std::string file = entry.path().filename().string();
			const bool kept = std::find(index["undo"].begin(), index["undo"].end(), file) != index["undo"].end()
				|| std::find(index["redo"].begin(), index["redo"].end(), file) != index["redo"].end();
			if (!kept && file != "index.json") std::filesystem::remove(entry.path(), code);
		}
		if (!write_file((std::filesystem::path(folder) / "index.json").string(), index))
		{
			error = "cannot write the history";
			return false;
		}
		return true;
	}

	bool LevelEditor::load_history(const std::string& folder, std::string& error)
	{
		m_undo.clear();
		m_redo.clear();
		m_done.clear();
		m_undoneOps.clear();
		const Json index = read_file((std::filesystem::path(folder) / "index.json").string());
		if (index.is_null() || index.is_discarded()) return true;   // no history yet
		for (const auto& file : index.value("undo", Json::array()))
		{
			Json state = read_file((std::filesystem::path(folder) / file.get<std::string>()).string());
			if (state.is_null() || state.is_discarded()) { error = "the history in '" + folder + "' is damaged"; return false; }
			m_undo.push_back(std::move(state));
		}
		for (const auto& file : index.value("redo", Json::array()))
		{
			Json state = read_file((std::filesystem::path(folder) / file.get<std::string>()).string());
			if (state.is_null() || state.is_discarded()) { error = "the history in '" + folder + "' is damaged"; return false; }
			m_redo.push_back(std::move(state));
		}
		for (const auto& op : index.value("done", Json::array())) m_done.push_back(op);
		for (const auto& op : index.value("undone", Json::array())) m_undoneOps.push_back(op);
		return true;
	}
}
