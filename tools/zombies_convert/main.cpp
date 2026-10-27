///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: main.cpp
// Purpose: Converts one Zombies level into Pakal's level format and cleans the models it uses.
//          Reads the intermediate JSON of a level, the game object library, and the converted
//          models of the legacy import. Writes level.json, the cleaned models and their textures.
//
// Usage: ZombiesConvert --level <Lvl.json> --library <gameobject-library.json>
//                       --imported <Imported/Legacy> --out <output dir>
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <functional>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{
	const double kUnitsPerMeter = 100.0;

	struct Options
	{
		fs::path level;
		fs::path library;
		fs::path imported;
		fs::path out;
		fs::path global;       // Lvl_HW_Global.json: zones the level includes
		fs::path runtimeDump;  // runtime dump of the level: exact object placement
		fs::path materials;    // folder with <model>.dae.legacy-materials.xml: blend technique per material
		fs::path collision;    // source.dae of zombiesDemo_col.bdae (the .bdae is a zip): collision outlines
		fs::path kit;          // folder of the open-source kit (glTF models, self-contained): roles may use them
		std::vector<std::pair<std::string, std::string>> roles; // role (player, enemy, pickup) -> model base name
	};

	std::string lower(std::string s)
	{
		std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return s;
	}

	std::string read_text(const fs::path& path)
	{
		std::ifstream in(path, std::ios::binary);
		std::stringstream buffer;
		buffer << in.rdbuf();
		return buffer.str();
	}

	void write_text(const fs::path& path, const std::string& text)
	{
		fs::create_directories(path.parent_path());
		std::ofstream out(path, std::ios::binary);
		out << text;
	}

	// Attribute value of a tag like: <channel source="#x" target="node/rotation"/>
	bool attribute_of(const std::string& tag, const std::string& name, std::string& value)
	{
		const std::string key = name + "=\"";
		const size_t at = tag.find(key);
		if (at == std::string::npos)
		{
			return false;
		}
		const size_t start = at + key.size();
		const size_t end = tag.find('"', start);
		if (end == std::string::npos)
		{
			return false;
		}
		value = tag.substr(start, end - start);
		return true;
	}

	// Removes every <open ...>...</open> block whose opening tag satisfies 'matches'
	std::string remove_blocks(const std::string& text, const std::string& openTag, const std::string& closeTag,
		const std::function<bool(const std::string&)>& matches, size_t& removed)
	{
		std::string out;
		size_t pos = 0;
		while (true)
		{
			const size_t open = text.find(openTag, pos);
			if (open == std::string::npos)
			{
				out.append(text, pos, std::string::npos);
				break;
			}
			const size_t openEnd = text.find('>', open);
			if (openEnd == std::string::npos)
			{
				out.append(text, pos, std::string::npos);
				break;
			}
			const std::string tag = text.substr(open, openEnd - open + 1);
			const bool selfClosing = tag.size() >= 2 && tag[tag.size() - 2] == '/';
			size_t blockEnd = openEnd + 1;
			if (!selfClosing)
			{
				const size_t close = text.find(closeTag, openEnd + 1);
				blockEnd = (close == std::string::npos) ? text.size() : close + closeTag.size();
			}

			if (matches(tag))
			{
				out.append(text, pos, open - pos);
				++removed;
			}
			else
			{
				out.append(text, pos, blockEnd - pos);
			}
			pos = blockEnd;
		}
		return out;
	}

	// Exporters write data Assimp does not accept. Removes it from the Collada copy:
	//  - <extra> blocks (exporter settings, also invalid inside <vertices>)
	//  - instance_effect references to other .bdae files
	//  - animation channels that do not target a node transform (e.g. AuraIntensity)
	// Some exporters bind the COLLADA namespace to a prefix ("ns0:COLLADA"). Assimp only reads the default
	// namespace, so the prefix is removed: xmlns:p="<COLLADA>" becomes xmlns="<COLLADA>" and "p:" goes.
	std::string strip_collada_prefix(const std::string& input)
	{
		const std::string schema = "http://www.collada.org/2005/11/COLLADASchema";
		const size_t decl = input.find("=\"" + schema + "\"");
		if (decl == std::string::npos)
		{
			return input;
		}
		const size_t start = input.rfind("xmlns:", decl);
		if (start == std::string::npos)
		{
			return input;
		}
		const std::string prefix = input.substr(start + 6, decl - (start + 6));
		if (prefix.empty())
		{
			return input;
		}
		std::string out = input;
		const std::string declText = "xmlns:" + prefix + "=\"" + schema + "\"";
		out.replace(out.find(declText), declText.size(), "xmlns=\"" + schema + "\"");
		const std::string open = "<" + prefix + ":";
		const std::string close = "</" + prefix + ":";
		for (size_t at = out.find(open); at != std::string::npos; at = out.find(open, at))
		{
			out.erase(at + 1, prefix.size() + 1);
		}
		for (size_t at = out.find(close); at != std::string::npos; at = out.find(close, at))
		{
			out.erase(at + 2, prefix.size() + 1);
		}
		return out;
	}

	std::string clean_collada(const std::string& raw, size_t& removed)
	{
		const std::string input = strip_collada_prefix(raw);
		std::string text = remove_blocks(input, "<extra", "</extra>",
			[](const std::string&) { return true; }, removed);

		text = remove_blocks(text, "<instance_effect", "</instance_effect>",
			[](const std::string& tag)
			{
				std::string url;
				return attribute_of(tag, "url", url) && url.find(".bdae#") != std::string::npos;
			}, removed);

		text = remove_blocks(text, "<channel", "</channel>",
			[](const std::string& tag)
			{
				std::string target;
				if (!attribute_of(tag, "target", target))
				{
					return false;
				}
				return target.find('/') == std::string::npos
					|| target.size() >= 9 && target.compare(target.size() - 9, 9, "Intensity") == 0;
			}, removed);

		return text;
	}

	struct Level
	{
		std::vector<std::string> wallLines;
		std::vector<std::string> enemyLines;
		std::vector<std::string> pickupLines;
		json ambientActors = json::array();
		json pickupSources = json::array();
		json destructibles = json::array(); // nodes with a Health component (props that break)
		std::vector<std::string> objectLines;
		bool hasSpawn = false;
		double spawnX = 0.0, spawnY = 0.0;
		int unresolvedMeshes = 0;
		std::map<std::string, int> unresolvedNames;
		std::map<std::string, fs::path> usedMeshes; // file name -> source .dae
		// Collision outlines of the level's collision mesh (zombiesDemo_col): one per linestrip, world
		// coordinates in level units. layer: "solid" (blocks everyone), "player" (blocks the player),
		// "sand" and "water" (sensors: no blocking), anything else keeps its material name.
		struct CollisionOutline
		{
			std::string layer;
			bool filled = false;
			std::vector<std::pair<double, double>> points;
		};
		std::vector<CollisionOutline> collisionOutlines;
		// Enemy spawners of the runtime dump (ZombieSpawner objects), world positions in level units
		std::vector<std::pair<double, double>> spawners;
		// Named spawn points of the level (legacy id -> world position): WarpPlayer and the like
		std::map<std::string, std::pair<double, double>> namedPoints;
		json cameraAnchors = json::object();
	};

	struct Library
	{
		std::map<std::string, std::string> meshFileByObject; // object name -> mesh base name (lower case)
		std::map<std::string, json> objectById;              // game object id -> its entry in the library
		std::map<std::string, json> objectByName;            // object (template) name -> its entry in the library
	};

	struct MeshIndex
	{
		std::map<std::string, fs::path> byBaseName; // lower-case base name -> .dae
	};

	bool has_value(const json& field)
	{
		return field.is_object() && field.value("hasValue", false);
	}

	double number(const json& field, const char* key)
	{
		return field.is_object() ? field.value(key, 0.0) : 0.0;
	}

	// Properties of a component as name -> value
	std::map<std::string, std::string> properties_of(const json& component)
	{
		std::map<std::string, std::string> props;
		if (component.contains("properties") && component["properties"].is_array())
		{
			for (const auto& p : component["properties"])
			{
				props[p.value("name", std::string())] = p.value("value", std::string());
			}
		}
		return props;
	}

	// "3000.000, 800.0000" -> {3000, 800}
	std::pair<double, double> pair_of(const std::string& text)
	{
		std::stringstream in(text);
		std::string a, b;
		std::getline(in, a, ',');
		std::getline(in, b, ',');
		return { std::atof(a.c_str()), std::atof(b.c_str()) };
	}

	// A property value as JSON: true/false and plain numbers become JSON values, the rest stays text
	json value_of(const std::string& text)
	{
		if (text == "true") return true;
		if (text == "false") return false;
		if (!text.empty())
		{
			char* end = nullptr;
			const double number = std::strtod(text.c_str(), &end);
			if (end && *end == '\0') return number;
		}
		return text;
	}

	// Properties of a component as a JSON object. A property with children is a table: its rows are
	// split where a name repeats ("Consumable, Chance, Consumable, Chance" -> two rows).
	json compact_properties(const json& properties)
	{
		json out = json::object();
		for (const auto& p : properties)
		{
			const std::string name = p.value("name", std::string());
			const json children = p.value("children", json::array());
			if (children.empty())
			{
				out[name] = value_of(p.value("value", std::string()));
				continue;
			}
			json rows = json::array();
			json row = json::object();
			for (const auto& child : children)
			{
				const std::string key = child.value("name", std::string());
				if (row.contains(key))
				{
					rows.push_back(row);
					row = json::object();
				}
				row[key] = child.value("children", json::array()).empty()
					? value_of(child.value("value", std::string())) : compact_properties(json::array({ child }))[key];
			}
			if (!row.empty()) rows.push_back(row);
			out[name] = rows;
		}
		return out;
	}

	// The gameplay components of an object (what the rules need: no mesh, animation or editor data)
	json gameplay_components(const json& object)
	{
		static const std::set<std::string> skipped = { "Base", "Mesh", "Animated" };
		json out = json::object();
		for (const auto& component : object.value("components", json::array()))
		{
			const std::string type = component.value("name", std::string());
			if (!skipped.count(type))
			{
				out[type] = compact_properties(component.value("properties", json::array()));
			}
		}
		return out;
	}

	Library read_library(const fs::path& path)
	{
		Library lib;
		const json root = json::parse(read_text(path));
		for (const auto& object : root.value("objects", json::array()))
		{
			const std::string name = object.value("name", std::string());
			const std::string objectId = object.value("id", std::string());
			if (!objectId.empty() && !lib.objectById.count(objectId))
			{
				lib.objectById[objectId] = object;
			}
			if (!name.empty() && !lib.objectByName.count(name))
			{
				lib.objectByName[name] = object;
			}
			if (name.empty() || lib.meshFileByObject.count(name))
			{
				continue;
			}
			for (const auto& component : object.value("components", json::array()))
			{
				if (component.value("name", std::string()) != "Mesh")
				{
					continue;
				}
				const auto props = properties_of(component);
				auto it = props.find("meshFile");
				if (it == props.end() || it->second.empty())
				{
					continue;
				}
				// "..\props\meshes\pr_gift.bdae" -> "pr_gift"
				fs::path file(it->second);
				lib.meshFileByObject[name] = lower(file.stem().string());
			}
		}
		return lib;
	}

	// Collada files come as <name>.dae or, for some actors, <name>.source.xml. Returns the model's base
	// name (lower case), or an empty string for other files.
	std::string collada_base_name(const fs::path& file)
	{
		const std::string name = lower(file.filename().string());
		const std::string sourceXml = ".source.xml";
		if (name.size() > 4 && name.compare(name.size() - 4, 4, ".dae") == 0)
		{
			return name.substr(0, name.size() - 4);
		}
		if (name.size() > sourceXml.size() && name.compare(name.size() - sourceXml.size(), sourceXml.size(), sourceXml) == 0)
		{
			return name.substr(0, name.size() - sourceXml.size());
		}
			if (name.size() > 5 && name.compare(name.size() - 5, 5, ".gltf") == 0)
			{
				return name.substr(0, name.size() - 5);
			}
		return std::string();
	}

	// Name of the cleaned copy in meshes/: always <base>.dae, so Assimp detects the format
	std::string output_file_name(const fs::path& source)
	{
		if (lower(source.extension().string()) == ".gltf")
		{
			return source.filename().string(); // glTF keeps its own name and format
		}
		std::string name = source.filename().string();
		const std::string base = collada_base_name(source);
		return name.substr(0, base.size()) + ".dae";
	}

	// <model>.dae.legacy-materials.xml files by model base name (lower case)
	std::map<std::string, fs::path> index_material_techniques(const fs::path& folder)
	{
		std::map<std::string, fs::path> index;
		if (folder.empty() || !fs::is_directory(folder))
		{
			return index;
		}
		const std::string suffix = ".dae.legacy-materials.xml";
		for (const auto& entry : fs::recursive_directory_iterator(folder))
		{
			const std::string name = lower(entry.path().filename().string());
			if (entry.is_regular_file() && name.size() > suffix.size()
				&& name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
			{
				index.emplace(name.substr(0, name.size() - suffix.size()), entry.path());
			}
		}
		return index;
	}

	// The game's shading technique of a material, as Pakal's blend mode and sidedness
	void blend_for_technique(const std::string& technique, std::string& blend, bool& doubleSided)
	{
		const std::string t = lower(technique);
		auto has = [&](const char* part) { return t.find(part) != std::string::npos; };
		doubleSided = has("doublesided");
		if (t.rfind("add", 0) == 0) blend = "additive";
		else if (has("alpha") || t.rfind("glass", 0) == 0) blend = "alpha";
		else if (t.rfind("modulate", 0) == 0) blend = "multiply";
		else if (t == "mask_mesh" || has("waving")) blend = "alphatest";
		else blend = "opaque";
		if (blend == "additive") doubleSided = true; // glows are seen from both sides
	}

	// Writes <model>.materials.xml (Pakal's material overrides) from the game's material list.
	// Returns false when every material is plain opaque, so no file is needed.
	bool write_material_overrides(const fs::path& legacyMaterials, const fs::path& out)
	{
		const std::string text = read_text(legacyMaterials);
		std::string body;
		size_t pos = 0;
		while ((pos = text.find("<material ", pos)) != std::string::npos)
		{
			const size_t end = text.find('>', pos);
			const std::string tag = text.substr(pos, end - pos);
			pos = end;
			std::string name, technique, tint;
			attribute_of(tag, "name", name);
			attribute_of(tag, "technique", technique);
			attribute_of(tag, "tint", tint);
			std::string blend;
			bool doubleSided = false;
			blend_for_technique(technique, blend, doubleSided);
			if (blend == "opaque" && !doubleSided && tint.empty())
			{
				continue;
			}
			body += "  <material name=\"" + name + "\" blend=\"" + blend + "\" doubleSided=\""
				+ (doubleSided ? "true" : "false") + "\"" + (tint.empty() ? "" : " tint=\"" + tint + "\"")
				+ " technique=\"" + technique + "\"/>\n";
		}
		if (body.empty())
		{
			return false;
		}
		write_text(out, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<materials>\n" + body + "</materials>\n");
		return true;
	}

		// Kit models (glTF, self-contained: buffers are embedded) by base name. A Collada model of the same
		// name from the level's imports keeps priority.
		void index_kit_models(const fs::path& kit, MeshIndex& index)
		{
			if (kit.empty() || !fs::is_directory(kit))
			{
				return;
			}
			size_t added = 0;
			for (const auto& entry : fs::recursive_directory_iterator(kit))
			{
				if (!entry.is_regular_file() || lower(entry.path().extension().string()) != ".gltf")
				{
					continue;
				}
				if (index.byBaseName.emplace(lower(entry.path().stem().string()), entry.path()).second)
				{
					++added;
				}
			}
			std::printf("kit models indexed: %zu from %s\n", added, kit.string().c_str());
		}

	MeshIndex index_meshes(const fs::path& imported)
	{
		MeshIndex index;
		for (const auto& entry : fs::recursive_directory_iterator(imported))
		{
			if (!entry.is_regular_file())
			{
				continue;
			}
			const std::string key = collada_base_name(entry.path());
			if (key.empty())
			{
				continue;
			}
			// A .dae wins over a .source.xml of the same model; otherwise the first one found wins
			auto it = index.byBaseName.find(key);
			if (it == index.byBaseName.end())
			{
				index.byBaseName.emplace(key, entry.path());
			}
			else if (lower(entry.path().extension().string()) == ".dae" && lower(it->second.extension().string()) != ".dae")
			{
				it->second = entry.path();
			}
		}
		return index;
	}

	std::string format_number(double v)
	{
		char buffer[64];
		std::snprintf(buffer, sizeof(buffer), "%.4f", v);
		return buffer;
	}

	// Navigation/collision meshes are data, not visible city geometry.
	bool is_auxiliary_geometry(const std::string& name, const std::string& meshBase)
	{
		return name == "LevelNav" || name == "LevelCollision"
			|| meshBase == "zombiesdemo_nav" || meshBase == "zombiesdemo_col";
	}

	// Legacy groups organize visibility/enabling, not spatial parenting (CGameObject::Init
	// explicitly does not SetParent). Authored Base positions already use level coordinates.
	void walk(const json& node, Level& level, const Library& library, const MeshIndex& meshes)
	{
		double px = 0.0, py = 0.0, pz = 0.0;
		if (has_value(node.value("position", json())))
		{
			const json& p = node["position"];
			px = number(p, "x");
			py = number(p, "y");
			pz = number(p, "z");
		}
		const double nodeScale = has_value(node.value("scale", json())) ? number(node["scale"], "x") : 1.0;
		const double acc = nodeScale;
		const double wx = px;
		const double wy = py;
		const double wz = pz;
		const std::string name = node.value("name", std::string());
		const std::string id = node.value("legacyId", std::string());
		if (!id.empty() && has_value(node.value("position", json())))
			level.cameraAnchors[id] = json::array({ wx, wy, wz });

		for (const auto& component : node.value("components", json::array()))
		{
			const std::string type = component.value("name", std::string());

			if (type == "BuiltinCollision2DPolygon")
			{
				const auto props = properties_of(component);
				if (props.count("CollisionType") && props.at("CollisionType") == "Trigger")
				{
					continue; // triggers are not solid
				}
				const auto size = pair_of(props.count("Size") ? props.at("Size") : "0,0");
				const std::string shape = props.count("Shape") ? props.at("Shape") : "";
				char line[256];
				if (shape == "Rect")
				{
					std::snprintf(line, sizeof(line), "%s %.2f %.2f %.2f %.2f %s", "rect", wx, wy, size.first * acc, size.second * acc,
						id.empty() ? "-1" : id.c_str());
					level.wallLines.push_back(line);
				}
				else if (shape == "Circle")
				{
					std::snprintf(line, sizeof(line), "%s %.2f %.2f %.2f %s", "circle", wx, wy, size.first * acc,
						id.empty() ? "-1" : id.c_str());
					level.wallLines.push_back(line);
				}
			}
			else if (type == "Idiot")
			{
				// Ambient path-following actors are not Enemy components. Counting them
				// as zombies prevents the Survival clear-wave condition from firing.
				level.ambientActors.push_back({ {"id", node.value("legacyId", std::string())},
					{"x", wx}, {"y", wy}, {"component", component} });
			}
			else if (type == "Health" && !id.empty())
			{
				// A prop that can be destroyed: its health, drops and blast (the gameplay components of the node)
				// A placed node only lists the properties it changes: start from its template in the library
				json components = json::object();
				auto found = library.objectByName.find(name);
				if (found != library.objectByName.end()) components = gameplay_components(found->second);
				for (auto& own : gameplay_components(node).items())
				{
					for (auto& property : own.value().items()) components[own.key()][property.key()] = property.value();
				}
				level.destructibles.push_back({ {"id", std::atoi(id.c_str())}, {"name", name}, {"x", wx}, {"y", wy},
					{"components", components} });
			}
			else if (type == "PickupSpawn")
			{
				// These are drop generators on props, not already spawned gifts.
				// Preserve their authored rules until destruction/timer drops are run.
				level.pickupSources.push_back({ {"id", node.value("legacyId", std::string())},
					{"x", wx}, {"y", wy}, {"component", component} });
			}
			else if (type == "Spawnpoint")
			{
				// Every spawn point is a named point; the first one is also the player's start (until the dump)
				level.namedPoints[node.value("legacyId", std::string())] = { wx, wy };
				if (!level.hasSpawn)
				{
					level.hasSpawn = true;
					level.spawnX = wx;
					level.spawnY = wy;
				}
			}
			else if (type == "Mesh")
			{
				// Zone nodes name their model in the component ("meshFile"); other nodes name it through the
				// game object library
				std::string baseName;
				const auto componentProps = properties_of(component);
				if (componentProps.count("meshFile"))
				{
					baseName = lower(fs::path(componentProps.at("meshFile")).stem().string());
				}
				if (baseName.empty() || !meshes.byBaseName.count(baseName))
				{
					auto found = library.meshFileByObject.find(name);
					if (found != library.meshFileByObject.end()) baseName = found->second;
				}
				auto mesh = meshes.byBaseName.find(baseName);
				if (is_auxiliary_geometry(name, baseName)) continue;
				if (mesh == meshes.byBaseName.end())
				{
					level.unresolvedMeshes++;
					level.unresolvedNames[name]++;
					continue;
				}
				const std::string fileName = output_file_name(mesh->second);
				level.usedMeshes[fileName] = mesh->second;

				const json rotation = node.value("rotation", json::object());
					const double rx = has_value(rotation) ? number(rotation, "x") : 0.0;
					const double ry = has_value(rotation) ? number(rotation, "y") : 0.0;
					const double rz = has_value(rotation) ? number(rotation, "z") : 0.0;
				std::string line = "{\"name\":\"" + name + "\",\"mesh\":\"meshes/" + fileName + "\""
					+ ",\"x\":" + format_number(wx) + ",\"y\":" + format_number(wy) + ",\"z\":" + format_number(wz)
					+ ",\"rot\":[" + format_number(rx) + "," + format_number(ry) + "," + format_number(rz) + "],\"scale\":" + format_number(acc) + "}";
				level.objectLines.push_back(line);
			}
		}

		for (const auto& child : node.value("children", json::array()))
		{
			walk(child, level, library, meshes);
		}
	}

	// Adds the library objects 'pending' names (game ids) to 'objects', with the objects they lead to: what a gift box
	// can give and what pickup spawners and managers drop
	void add_rule_objects(const Library& library, json& objects, std::vector<std::string> pending)
	{
		auto add_ids = [&pending](const json& rows, const char* key)
		{
			if (!rows.is_array()) return;
			for (const auto& row : rows)
			{
				if (row.contains(key) && row[key].is_number()) pending.push_back(std::to_string(row[key].get<long long>()));
			}
		};
		while (!pending.empty())
		{
			const std::string id = pending.back();
			pending.pop_back();
			if (objects.contains(id)) continue;
			auto found = library.objectById.find(id);
			if (found == library.objectById.end())
			{
				std::printf("rules: object %s is not in the library\n", id.c_str());
				continue;
			}
			const json components = gameplay_components(found->second);
			objects[id] = { {"name", found->second.value("name", std::string())}, {"components", components} };
			// the air strike's effect is code in the original (no Market function says it): the game reads it from data
			if (objects[id]["name"] == "AirStrikeBoost") objects[id]["components"]["Effect"] = { {"Type", "airstrike"} };
			add_ids(components.value("GiftBox", json::object()).value("Consumables", json::array()), "Consumable");
			add_ids(components.value("PickupSpawn", json::object()).value("Pickups", json::array()), "Pickup");
			add_ids(components.value("PickupManager", json::object()).value("Pickups", json::array()), "Pickup");
		}
	}

	// Game rules from the object library: the objects the level's gameplay reads (weapon, player, enemies,
	// money, the gift box and what it can give, and the pickups props drop), by game id
	json rules_of(const Library& library, const Level& level)
	{
		std::vector<std::string> pending = {
			"15001", // Glock: the starting weapon
			"35001", // Player
			"25001", // ZombieBusiness
			"25009", // Paparazzi
			"25010", "25011", "25012", "25013", "25016", // the special zombies: Tourist, Zombinator, DogWalker, ZombieDog, Boomer
			"25008", // PickupManager: what random drops can be
			"15031", // GiftBox
		};
		auto add_ids = [&pending](const json& rows, const char* key)
		{
			if (!rows.is_array()) return;
			for (const auto& row : rows)
			{
				if (row.contains(key) && row[key].is_number()) pending.push_back(std::to_string(row[key].get<long long>()));
			}
		};
		for (const auto& prop : level.destructibles)
		{
			add_ids(prop["components"].value("PickupSpawn", json::object()).value("Pickups", json::array()), "Pickup");
		}

		json objects = json::object();
		add_rule_objects(library, objects, pending);
		std::printf("rules: %zu objects\n", objects.size());
		return { {"startingWeapon", 15001}, {"player", 35001}, {"giftBox", 15031}, {"pickupManager", 25008}, {"objects", objects} };
	}

	void copy_textures(const fs::path& modelFile, const fs::path& outMeshes)
	{
		// Textures sit in a Textures folder next to the model; the model resolves them by file name
		const fs::path textures = modelFile.parent_path() / "Textures";
		if (!fs::is_directory(textures))
		{
			return;
		}
		for (const auto& entry : fs::directory_iterator(textures))
		{
			if (entry.is_regular_file() && lower(entry.path().extension().string()) != ".meta")
			{
				fs::copy_file(entry.path(), outMeshes / entry.path().filename(), fs::copy_options::skip_existing);
			}
		}
	}

	// Object placement from the game's runtime dump: exact world positions, Euler rotations and scales
	// Layer of a collision outline, from the material name the level's collision mesh gives it
	std::string collision_layer(const std::string& material)
	{
		if (material == "ColorMaterial") return "solid";
		if (material == "player") return "player";
		if (material == "matDirt") return "solid"; // compiled Hollywood payload: collision type -1
		if (material == "water") return "water";
		return material;
	}

	// Collision outlines of the level's collision mesh. zombiesdemo_col.bdae is a zip whose source.dae is
	// plain Collada: one geometry per outline (linestrips of 'p' point indices), placed by its node
	// (translate, axis-angle rotation; the pivot only scales Z, which is ignored in 2D).
	// Objectives of the level in the order the scene runs them (the Scene component's Objectives list)
	void collect_scene_objectives(const json& node, std::vector<std::string>& order)
	{
		for (const auto& component : node.value("components", json::array()))
		{
			if (component.value("name", std::string()) != "Scene" || !order.empty())
			{
				continue;
			}
			for (const auto& property : component.value("properties", json::array()))
			{
				if (property.value("name", std::string()) != "Objectives")
				{
					continue;
				}
				for (const auto& element : property.value("elements", json::array()))
				{
					for (const auto& child : element.value("properties", json::array()))
					{
						if (child.value("name", std::string()) == "objectiveId")
						{
							order.push_back(child.value("value", std::string()));
						}
					}
				}
			}
		}
		for (const auto& child : node.value("children", json::array()))
		{
			collect_scene_objectives(child, order);
		}
	}

	// Objective scripts of the level (Objective components): OnActivate, Run and OnDeactivate, by objective id
	void collect_objectives(const json& node, json& objectives)
	{
		for (const auto& component : node.value("components", json::array()))
		{
			if (component.value("name", std::string()) != "Objective")
			{
				continue;
			}
			std::string id;
			json entry = json::object();
			for (const auto& property : component.value("properties", json::array()))
			{
				const std::string name = property.value("name", std::string());
				if (name == "Id")
				{
					id = property.value("value", std::string());
				}
				else if (name == "Script")
				{
					for (const auto& script : property.value("scripts", json::array()))
					{
						const std::string scriptName = script.value("name", std::string());
						const std::string key = scriptName == "OnActivate" ? "onActivate"
							: scriptName == "Run" ? "run" : scriptName == "OnDeactivate" ? "onDeactivate" : std::string();
						if (!key.empty() && !entry.contains(key))
						{
							entry[key] = script.value("code", std::string());
						}
					}
				}
			}
			if (!id.empty() && !entry.empty() && !objectives.contains(id))
			{
				objectives[id] = entry;
			}
		}
		for (const auto& child : node.value("children", json::array()))
		{
			collect_objectives(child, objectives);
		}
	}

	void read_collision_collada(const fs::path& path, Level& level)
	{
		if (lower(path.extension().string()) == ".json")
		{
			const json source = json::parse(read_text(path));
			if (source.at("format") != "zombies-compiled-collision" || source.at("version") != 1
				|| source.at("units_per_meter") != 100) throw std::runtime_error("Unsupported compiled collision manifest");
			for (const auto& polygon : source.at("segments"))
			{
				const int kind = polygon.at("legacy_type").get<int>();
				const std::string layer = kind == -1 ? "solid" : kind == 0 ? "player" : kind == 6 ? "water" : "";
				if (layer.empty() || polygon.at("layer") != layer) throw std::runtime_error("Invalid source collision type");
				Level::CollisionOutline outline;
				outline.layer = layer;
				outline.filled = true;
				for (const auto& point : polygon.at("points"))
					outline.points.push_back({ point.at(0).get<double>(), point.at(1).get<double>() });
				if (outline.points.size() < 3 || outline.points.size() > 8) throw std::runtime_error("Invalid collision polygon");
				level.collisionOutlines.push_back(std::move(outline));
			}
			std::printf("compiled collision: %zu authoritative polygons\n", level.collisionOutlines.size());
			return;
		}
		const std::string text = read_text(path);
		const std::string geometryOpen = "<geometry id=\"";
		size_t cursor = 0;
		size_t outlines = 0;
		while ((cursor = text.find(geometryOpen, cursor)) != std::string::npos)
		{
			const size_t idStart = cursor + geometryOpen.size();
			const std::string id = text.substr(idStart, text.find('"', idStart) - idStart);
			const size_t end = text.find("</geometry>", cursor);
			const std::string block = text.substr(cursor, end - cursor);
			cursor = end;

			// Vertex positions: the first float_array of the geometry
			const size_t arrayStart = block.find("<float_array");
			const size_t valuesStart = block.find('>', arrayStart) + 1;
			const size_t valuesEnd = block.find('<', valuesStart);
			std::istringstream numbers(block.substr(valuesStart, valuesEnd - valuesStart));
			std::vector<double> positions;
			for (double v = 0.0; numbers >> v;) positions.push_back(v);

			// Node transform: "<id without -mesh>-node" followed by translate and rotate
			const std::string baseName = id.size() > 5 ? id.substr(0, id.size() - 5) : id;
			double tx = 0.0, ty = 0.0;
			double ax = 0.0, ay = 0.0, az = 1.0, angle = 0.0;
			const size_t nodeAt = text.find("<node id=\"" + baseName + "-node\"");
			if (nodeAt != std::string::npos)
			{
				const std::string around = text.substr(nodeAt, 400);
				const size_t tAt = around.find("<translate>");
				if (tAt != std::string::npos)
				{
					std::istringstream t(around.substr(tAt + 11));
					t >> tx >> ty;
				}
				const size_t rAt = around.find("<rotate>");
				if (rAt != std::string::npos)
				{
					std::istringstream r(around.substr(rAt + 8));
					r >> ax >> ay >> az >> angle;
				}
			}
			const double radians = angle * 3.14159265358979323846 / 180.0;
			const double c = std::cos(radians), s = std::sin(radians);
			const double axisLength = std::sqrt(ax * ax + ay * ay + az * az);
			const double kx = axisLength > 0.0 ? ax / axisLength : 0.0;
			const double ky = axisLength > 0.0 ? ay / axisLength : 0.0;
			const double kz = axisLength > 0.0 ? az / axisLength : 1.0;

			// Each linestrips element: its material and its point indices
			size_t strip = 0;
			while ((strip = block.find("<linestrips", strip)) != std::string::npos)
			{
				const size_t materialAt = block.find("material=\"", strip) + 10;
				const std::string material = block.substr(materialAt, block.find('"', materialAt) - materialAt);
				const size_t pStart = block.find("<p>", strip) + 3;
				const size_t pEnd = block.find("</p>", pStart);
				std::istringstream indices(block.substr(pStart, pEnd - pStart));
				std::vector<size_t> order;
				for (size_t v = 0; indices >> v;) order.push_back(v);
				strip = pEnd;

				Level::CollisionOutline outline;
				outline.layer = collision_layer(material);
				for (size_t index : order)
				{
					if (3 * index + 2 >= positions.size()) continue;
					const double px = positions[3 * index], py = positions[3 * index + 1], pz = positions[3 * index + 2];
					// Rodrigues rotation of (px, py, pz) about the unit axis (kx, ky, kz)
					const double dot = kx * px + ky * py + kz * pz;
					const double cx = ky * pz - kz * py, cy = kz * px - kx * pz;
					const double rx = px * c + cx * s + kx * dot * (1.0 - c);
					const double ry = py * c + cy * s + ky * dot * (1.0 - c);
					outline.points.push_back({ tx + rx, ty + ry });
				}
				if (outline.points.size() >= 2)
				{
					level.collisionOutlines.push_back(outline);
					++outlines;
				}
			}
		}
		std::printf("collision outlines: %zu from %s\n", outlines, path.filename().string().c_str());
	}

	void place_from_dump(const fs::path& dumpPath, Level& level, const MeshIndex& meshes)
	{
		const json dump = json::parse(read_text(dumpPath));
		level.objectLines.clear();
		level.usedMeshes.clear();
		level.unresolvedMeshes = 0;
		level.unresolvedNames.clear();

		// childIds are logical ownership, not spatial parenting. The original runtime
		// uses Base()->GetPos() directly for actors, physics and spawners.
		auto world_position = [](const json& object, double out[3])
		{
			const json p = object.value("position", json::object());
			out[0] = number(p, "x");
			out[1] = number(p, "y");
			out[2] = number(p, "z");
		};
		for (const auto& object : dump.value("objects", json::array()))
		{
			const long long objectId = object.value("id", -1LL);
			if (objectId >= 0 && object.value("hasBase", false))
			{
				double at[3];
				world_position(object, at);
				level.cameraAnchors[std::to_string(objectId)] = json::array({ at[0], at[1], at[2] });
			}
			// The game's own player start replaces the spawn guessed from the level data
			if (object.value("templateName", std::string()) == "Player")
			{
				double at[3];
				world_position(object, at);
				level.spawnX = at[0];
				level.spawnY = at[1];
				level.hasSpawn = true;
			}
			// Enemy spawners: where the game's zombies appear (world position, level units)
			if (object.value("templateName", std::string()) == "ZombieSpawner")
			{
				double at[3];
				world_position(object, at);
				level.spawners.push_back({ at[0], at[1] });
			}
			const std::string meshFile = object.value("meshFile", std::string());
			if (meshFile.empty())
			{
				continue;
			}
			const std::string name = object.value("templateName", std::string());
			if (is_auxiliary_geometry(name, lower(fs::path(meshFile).stem().string()))) continue;
			// Characters are entities of their own (enemies, player), not static scenery
			if (name == "WanderingIdiot" || name == "Player" || lower(fs::path(meshFile).stem().string()).rfind("zombie_", 0) == 0)
			{
				continue;
			}
			auto mesh = meshes.byBaseName.find(lower(fs::path(meshFile).stem().string()));
			if (mesh == meshes.byBaseName.end())
			{
				level.unresolvedMeshes++;
				level.unresolvedNames[name]++;
				continue;
			}
			const std::string fileName = output_file_name(mesh->second);
			level.usedMeshes[fileName] = mesh->second;

			double world[3];
			world_position(object, world);
			json rotation = object.value("rotationEulerDegrees", json::object());
			json scale = object.value("scale", json::object());
			// Level pieces are modelled in world space: their vertices already hold level coordinates,
			// so the object's own transform (and its parent's) must not be applied.
			const std::string stem = lower(fs::path(meshFile).stem().string());
			if (stem.rfind("lv_", 0) == 0 || stem.rfind("props_", 0) == 0 || stem.rfind("zombiesdemo_", 0) == 0
				|| stem.rfind("zwood", 0) == 0 || stem.rfind("fx_tiremark", 0) == 0)
			{
				world[0] = world[1] = world[2] = 0.0;
				rotation = json::object();
				scale = json{ { "x", 1.0 }, { "y", 1.0 }, { "z", 1.0 } };
			}
			std::string line = "{\"name\":\"" + name + "\",\"id\":" + std::to_string(object.value("id", -1LL)) + ",\"mesh\":\"meshes/" + fileName + "\""
				+ ",\"x\":" + format_number(world[0])
				+ ",\"y\":" + format_number(world[1])
				+ ",\"z\":" + format_number(world[2])
				+ ",\"rot\":[" + format_number(number(rotation, "x")) + "," + format_number(number(rotation, "y"))
				+ "," + format_number(number(rotation, "z")) + "],\"scale\":" + format_number(scale.is_object() ? number(scale, "x") : 1.0) + "}";
			level.objectLines.push_back(line);
		}
	}

	// World-space pieces of the level (lv_* streets, parks, outskirts) are modelled in level coordinates, so
	// they are placed with no transform. The runtime dump lists some of them; the zone data lists more.
	// The ones the dump does not list are added from the zone data, so the city has no holes.
	void add_missing_world_pieces(const std::vector<std::string>& zoneLines, Level& level, const MeshIndex& meshes)
	{
		auto mesh_of = [](const std::string& line)
		{
			const size_t at = line.find("\"mesh\":\"meshes/");
			if (at == std::string::npos) return std::string();
			const size_t start = at + 15;
			return line.substr(start, line.find('"', start) - start);
		};
		auto is_world_piece = [](const std::string& file)
		{
			const std::string base = lower(fs::path(file).stem().string());
			return !is_auxiliary_geometry("", base)
				&& (base.rfind("lv_", 0) == 0 || base.rfind("zombiesdemo_", 0) == 0);
		};

		std::set<std::string> placed;
		for (const auto& line : level.objectLines)
		{
			placed.insert(mesh_of(line));
		}
		size_t added = 0;
		for (const auto& line : zoneLines)
		{
			const std::string file = mesh_of(line);
			if (file.empty() || !is_world_piece(file) || placed.count(file))
			{
				continue;
			}
			placed.insert(file);
			// Same line with the placement replaced by the identity transform
			const size_t placement = line.find(",\"x\":");
			if (placement == std::string::npos) continue;
			level.objectLines.push_back(line.substr(0, placement)
				+ ",\"x\":0.0000,\"y\":0.0000,\"z\":0.0000,\"rot\":[0.0000,0.0000,0.0000],\"scale\":1.0000}");
			auto mesh = meshes.byBaseName.find(lower(fs::path(file).stem().string()));
			if (mesh != meshes.byBaseName.end())
			{
				level.usedMeshes[output_file_name(mesh->second)] = mesh->second;
			}
			++added;
		}
		std::printf("world pieces added from the zones: %zu\n", added);
	}

	// Top-down map of the converted level (SVG, level units). Use it to check placement by eye:
	// walls gray, objects blue, enemies red, pickups gold, spawn green. North is up (+Y).
	void write_map_svg(const fs::path& path, const Level& level)
	{
		struct Dot { double x, y; };
		std::vector<Dot> objects, enemies, pickups;
		for (const auto& line : level.objectLines)
		{
			const json o = json::parse(line);
			objects.push_back({ o.value("x", 0.0), o.value("y", 0.0) });
		}
		for (const auto& line : level.enemyLines)
		{
			std::istringstream in(line);
			double x = 0.0, y = 0.0;
			in >> x >> y;
			enemies.push_back({ x, y });
		}
		for (const auto& line : level.pickupLines)
		{
			std::istringstream in(line);
			double x = 0.0, y = 0.0;
			in >> x >> y;
			pickups.push_back({ x, y });
		}

		double minX = 1e18, minY = 1e18, maxX = -1e18, maxY = -1e18;
		auto extend = [&](double x, double y)
		{
			minX = std::min(minX, x); minY = std::min(minY, y);
			maxX = std::max(maxX, x); maxY = std::max(maxY, y);
		};
		for (const auto& d : objects) extend(d.x, d.y);
		for (const auto& d : enemies) extend(d.x, d.y);
		for (const auto& d : pickups) extend(d.x, d.y);
		if (level.hasSpawn) extend(level.spawnX, level.spawnY);
		if (minX > maxX) return;
		const double margin = 1000.0; // 10 m
		minX -= margin; minY -= margin; maxX += margin; maxY += margin;
		const double width = 1400.0;
		const double height = width * (maxY - minY) / (maxX - minX);
		const double scale = width / (maxX - minX);

		// Map coordinates: x to the right, y up (flipped for SVG)
		auto sx = [&](double x) { return (x - minX) * scale; };
		auto sy = [&](double y) { return (maxY - y) * scale; };

		std::ostringstream svg;
		svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width << "\" height=\"" << height
			<< "\" viewBox=\"0 0 " << width << " " << height << "\">\n";
		svg << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";
		for (const auto& line : level.wallLines)
		{
			std::istringstream in(line);
			std::string kind;
			double x = 0.0, y = 0.0, a = 0.0, b = 0.0;
			in >> kind >> x >> y >> a;
			if (kind == "rect")
			{
				in >> b;
				svg << "<rect x=\"" << sx(x - a / 2) << "\" y=\"" << sy(y + b / 2) << "\" width=\"" << a * scale
					<< "\" height=\"" << b * scale << "\" fill=\"#777\" fill-opacity=\"0.8\"/>\n";
			}
			else
			{
				svg << "<circle cx=\"" << sx(x) << "\" cy=\"" << sy(y) << "\" r=\"" << a / 2 * scale << "\" fill=\"#777\"/>\n";
			}
		}
		for (const auto& d : objects)
		{
			svg << "<circle cx=\"" << sx(d.x) << "\" cy=\"" << sy(d.y) << "\" r=\"2.5\" fill=\"#2850dc\"/>\n";
		}
		for (const auto& d : pickups)
		{
			svg << "<circle cx=\"" << sx(d.x) << "\" cy=\"" << sy(d.y) << "\" r=\"3\" fill=\"gold\"/>\n";
		}
		for (const auto& d : enemies)
		{
			svg << "<circle cx=\"" << sx(d.x) << "\" cy=\"" << sy(d.y) << "\" r=\"5\" fill=\"red\"/>\n";
		}
		if (level.hasSpawn)
		{
			svg << "<circle cx=\"" << sx(level.spawnX) << "\" cy=\"" << sy(level.spawnY) << "\" r=\"7\" fill=\"limegreen\"/>\n";
		}
		svg << "</svg>\n";
		write_text(path, svg.str());
	}

	bool parse_options(int argc, char* argv[], Options& options)
	{
		for (int i = 1; i + 1 < argc; i += 2)
		{
			const std::string flag = argv[i];
			const fs::path value = argv[i + 1];
			if (flag == "--level") options.level = value;
			else if (flag == "--library") options.library = value;
			else if (flag == "--imported") options.imported = value;
			else if (flag == "--out") options.out = value;
			else if (flag == "--global") options.global = value;
			else if (flag == "--runtime-dump") options.runtimeDump = value;
			else if (flag == "--materials") options.materials = value;
			else if (flag == "--collision") options.collision = value;
			else if (flag == "--kit") options.kit = value;
			else if (flag == "--role")
			{
				const std::string spec = value.string();
				const size_t eq = spec.find('=');
				if (eq == std::string::npos) return false;
				options.roles.push_back({ spec.substr(0, eq), spec.substr(eq + 1) });
			}
			else return false;
		}
		return !options.level.empty() && !options.library.empty() && !options.imported.empty() && !options.out.empty();
	}
}

// ZombiesConvert --library <gameobject-library.json> --add-rules <level.json> --rule-ids 25010,25016,...
// adds those library objects (enemy types, weapons...) to the rules of an already converted level, nothing else
int add_rules_only(int argc, char* argv[])
{
	fs::path library, levelPath;
	std::string ids;
	for (int i = 1; i + 1 < argc; i += 2)
	{
		const std::string flag = argv[i];
		if (flag == "--library") library = argv[i + 1];
		else if (flag == "--add-rules") levelPath = argv[i + 1];
		else if (flag == "--rule-ids") ids = argv[i + 1];
	}
	if (library.empty() || levelPath.empty() || ids.empty())
	{
		std::cerr << "Usage: ZombiesConvert --library <gameobject-library.json> --add-rules <level.json> --rule-ids <id,id,...>" << std::endl;
		return 2;
	}
	json level = json::parse(read_text(levelPath));
	std::vector<std::string> pending;
	for (size_t start = 0; start < ids.size();)
	{
		const size_t comma = ids.find(',', start);
		pending.push_back(ids.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
		start = comma == std::string::npos ? ids.size() : comma + 1;
	}
	json& objects = level["rules"]["objects"];
	const size_t before = objects.size();
	add_rule_objects(read_library(library), objects, pending);
	std::printf("rules: %zu objects added\n", objects.size() - before);
	write_text(levelPath, level.dump(1));
	return 0;
}

int main(int argc, char* argv[])
{
	for (int i = 1; i < argc; ++i)
	{
		if (std::string(argv[i]) == "--add-rules") return add_rules_only(argc, argv);
	}
	Options options;
	if (!parse_options(argc, argv, options))
	{
		std::cerr << "Usage: ZombiesConvert --level <Lvl.json> --library <gameobject-library.json> "
			"--imported <Imported/Legacy> --out <output dir>" << std::endl;
		return 2;
	}

	const json levelJson = json::parse(read_text(options.level));
	const Library library = read_library(options.library);
	MeshIndex meshes = index_meshes(options.imported);
	index_kit_models(options.kit, meshes);

	Level level;
	for (const auto& top : levelJson.value("nodes", json::array()))
	{
		walk(top, level, library, meshes);
	}

	// Zones of the global file that this level includes: their walls, enemies and pickups
	if (!options.global.empty())
	{
		std::set<std::string> zoneIds;
		for (const auto& include : levelJson.value("includes", json::array()))
		{
			for (const auto& zone : include.value("zoneIds", json::array()))
			{
				zoneIds.insert(zone.get<std::string>());
			}
		}
		const json globalJson = json::parse(read_text(options.global));
		size_t zoneNodes = 0;
		for (const auto& top : globalJson.value("nodes", json::array()))
		{
			const json id = top.value("legacyId", json());
			std::string idText;
			if (id.is_string()) idText = id.get<std::string>();
			else if (id.is_number_integer()) idText = std::to_string(id.get<long long>());
			if (zoneIds.count(idText))
			{
				walk(top, level, library, meshes);
				++zoneNodes;
			}
		}
		std::printf("zones: %zu of %zu included from %s\n", zoneNodes, zoneIds.size(), options.global.filename().string().c_str());
	}

	// Placement of objects: the runtime dump is the reference when it is given
	if (!options.runtimeDump.empty())
	{
		const std::vector<std::string> zoneLines = level.objectLines; // from the walk of the level and its zones
		place_from_dump(options.runtimeDump, level, meshes);
		add_missing_world_pieces(zoneLines, level, meshes);
		std::printf("placement from runtime dump %s\n", options.runtimeDump.filename().string().c_str());
	}

	// Models the game uses for its roles (player, enemies, pickups), resolved like object meshes
	std::map<std::string, std::string> roleFiles; // role -> file name in meshes/
	for (const auto& role : options.roles)
	{
		auto mesh = meshes.byBaseName.find(lower(role.second));
		if (mesh == meshes.byBaseName.end())
		{
			std::printf("role %s: model '%s' not found\n", role.first.c_str(), role.second.c_str());
			continue;
		}
		const std::string fileName = output_file_name(mesh->second);
		level.usedMeshes[fileName] = mesh->second;
		roleFiles[role.first] = fileName;
	}

	// Copy the cleaned models and their textures
	const fs::path outMeshes = options.out / "meshes";
	fs::create_directories(outMeshes);
	size_t cleanedBlocks = 0;
	const std::map<std::string, fs::path> techniques = index_material_techniques(options.materials);
	size_t materialFiles = 0;
	for (const auto& used : level.usedMeshes)
	{
		if (lower(used.second.extension().string()) == ".gltf")
		{
			// glTF is not Collada: nothing to clean, and its buffers are embedded in the file
			fs::copy_file(used.second, outMeshes / used.first, fs::copy_options::overwrite_existing);
			continue;
		}
		const std::string source = read_text(used.second);
		const std::string cleaned = clean_collada(source, cleanedBlocks);
		write_text(outMeshes / used.first, cleaned);
		copy_textures(used.second, outMeshes);

		auto technique = techniques.find(collada_base_name(used.second));
		if (technique != techniques.end() && write_material_overrides(technique->second, outMeshes / (used.first + ".materials.xml")))
		{
			++materialFiles;
		}
	}
	if (!options.materials.empty())
	{
		std::printf("material overrides: %zu models\n", materialFiles);
	}

	if (!options.collision.empty())
	{
		read_collision_collada(options.collision, level);
	}

	// level.json in Pakal's format
	std::string text;
	text += "{\n";
	text += "  \"format\": \"pakal-level\",\n  \"version\": 1,\n";
	text += "  \"name\": \"" + options.level.stem().string() + "\",\n";
	text += "  \"roles\": {";
	{
		bool first = true;
		for (const auto& role : roleFiles)
		{
			text += std::string(first ? "" : ",") + "\"" + role.first + "\":\"meshes/" + role.second + "\"";
			first = false;
		}
	}
	text += "},\n";
	text += "  \"mode\": \"" + levelJson.value("summary", json::object()).value("modeHint", std::string()) + "\",\n";
	text += "  \"units_per_meter\": 100,\n";
	text += "  \"spawn\": [" + format_number(level.spawnX) + ", " + format_number(level.spawnY) + "],\n";

	text += "  \"walls\": [\n";
	for (size_t i = 0; i < level.wallLines.size(); ++i)
	{
		std::istringstream in(level.wallLines[i]);
		std::string kind;
		double x = 0.0, y = 0.0, a = 0.0, b = 0.0;
		int wallId = -1;
		in >> kind >> x >> y >> a;
		char item[192];
		if (kind == "rect")
		{
			in >> b >> wallId;
			std::snprintf(item, sizeof(item), "{\"shape\":\"rect\",\"x\":%.2f,\"y\":%.2f,\"w\":%.2f,\"h\":%.2f,\"id\":%d}", x, y, a, b, wallId);
		}
		else
		{
			in >> wallId;
			std::snprintf(item, sizeof(item), "{\"shape\":\"circle\",\"x\":%.2f,\"y\":%.2f,\"d\":%.2f,\"id\":%d}", x, y, a, wallId);
		}
		text += std::string("    ") + item + (i + 1 < level.wallLines.size() ? ",\n" : "\n");
	}
	text += "  ],\n";

	// Scripts of the level: the objectives' Lua hooks, kept as text (the module runs them)
	json objectives = json::object();
	for (const auto& top : levelJson.value("nodes", json::array()))
	{
		collect_objectives(top, objectives);
	}
	// Order the scene runs them in (the objectives' ids)
	std::vector<std::string> sceneOrder;
	for (const auto& top : levelJson.value("nodes", json::array())) collect_scene_objectives(top, sceneOrder);
	json order = json::array();
	for (const auto& id : sceneOrder) order.push_back(id);
	json points = json::object();
	for (const auto& point : level.namedPoints)
	{
		points[point.first] = json::array({ point.second.first, point.second.second });
	}
	text += "  \"script\": {\"order\": " + order.dump() + ", \"points\": " + points.dump()
		+ ", \"anchors\": " + level.cameraAnchors.dump() + ", \"objectives\": " + objectives.dump() + "},\n";

	// Enemy spawners: the game's own spawn points (positions from the runtime dump). Interval and enemy
	// type come from the level script (SetEnemySpawn), not from the spawner.
	text += "  \"spawners\": [\n";
	for (size_t i = 0; i < level.spawners.size(); ++i)
	{
		text += "    {\"x\":" + format_number(level.spawners[i].first) + ",\"y\":" + format_number(level.spawners[i].second) + "}"
			+ (i + 1 < level.spawners.size() ? ",\n" : "\n");
	}
	text += "  ],\n";

	// Collision outlines (level units): the module makes a static edge chain of each
	text += "  \"collision\": {\"segments\": [\n";
	for (size_t i = 0; i < level.collisionOutlines.size(); ++i)
	{
		const auto& outline = level.collisionOutlines[i];
		std::string points;
		for (size_t p = 0; p < outline.points.size(); ++p)
		{
			points += "[" + format_number(outline.points[p].first) + "," + format_number(outline.points[p].second) + "]"
				+ (p + 1 < outline.points.size() ? "," : "");
		}
		text += "    {\"layer\":\"" + outline.layer + "\",\"filled\":" + (outline.filled ? "true" : "false") + ",\"points\":[" + points + "]}"
			+ (i + 1 < level.collisionOutlines.size() ? ",\n" : "\n");
	}
	text += "  ]},\n";

	text += "  \"ambientActors\": " + level.ambientActors.dump() + ",\n";
	text += "  \"pickupSources\": " + level.pickupSources.dump() + ",\n";
	text += "  \"destructibles\": " + level.destructibles.dump() + ",\n";
	text += "  \"rules\": " + rules_of(library, level).dump() + ",\n";
	text += "  \"enemies\": [\n";
	for (size_t i = 0; i < level.enemyLines.size(); ++i)
	{
		std::istringstream in(level.enemyLines[i]);
		double x = 0.0, y = 0.0, speed = 0.0;
		in >> x >> y >> speed;
		char item[160];
		std::snprintf(item, sizeof(item), "{\"x\":%.2f,\"y\":%.2f,\"speed\":%.2f}", x, y, speed);
		text += std::string("    ") + item + (i + 1 < level.enemyLines.size() ? ",\n" : "\n");
	}
	text += "  ],\n";

	text += "  \"pickups\": [\n";
	for (size_t i = 0; i < level.pickupLines.size(); ++i)
	{
		std::istringstream in(level.pickupLines[i]);
		double x = 0.0, y = 0.0;
		in >> x >> y;
		char item[96];
		std::snprintf(item, sizeof(item), "{\"x\":%.2f,\"y\":%.2f}", x, y);
		text += std::string("    ") + item + (i + 1 < level.pickupLines.size() ? ",\n" : "\n");
	}
	text += "  ],\n";

	text += "  \"objects\": [\n";
	for (size_t i = 0; i < level.objectLines.size(); ++i)
	{
		text += "    " + level.objectLines[i] + (i + 1 < level.objectLines.size() ? ",\n" : "\n");
	}
	text += "  ]\n}\n";

	write_text(options.out / "level.json", text);
	write_map_svg(options.out / "level_map.svg", level);

	std::printf("level: %s\n", options.level.filename().string().c_str());
	std::printf("walls %zu, enemies %zu, pickups %zu, objects %zu, meshes %zu\n",
		level.wallLines.size(), level.enemyLines.size(), level.pickupLines.size(),
		level.objectLines.size(), level.usedMeshes.size());
	std::printf("spawn %s, cleaned blocks %zu\n", level.hasSpawn ? "found" : "MISSING", cleanedBlocks);
	if (level.unresolvedMeshes > 0)
	{
		std::printf("unresolved meshes: %d\n", level.unresolvedMeshes);
		for (const auto& name : level.unresolvedNames)
		{
			std::printf("  %s x%d\n", name.first.c_str(), name.second);
		}
	}
	return level.hasSpawn ? 0 : 1;
}
