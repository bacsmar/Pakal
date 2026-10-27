///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelEditor.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "LevelEditor.h"

#include <algorithm>
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
			if (number(op, "yaw", value)) o.rot[2] = value;
			else if (number(op, "by", value)) o.rot[2] += value;
			else return "turn needs yaw or by";
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
