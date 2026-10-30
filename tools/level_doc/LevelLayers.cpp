///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelLayers.cpp
// Purpose: A level made of a base and a layer over it - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "LevelLayers.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

namespace Pakal
{
	namespace
	{
		namespace fs = std::filesystem;

		// Sections whose entries add up (the base's and the layer's); the layer's objects are handled on their own
		const char* kAdded[] = { "walls", "enemies", "pickups", "zones", "destructibles" };
		// What a layer says about itself, not a section of the level
		const char* kLayerOnly[] = { "extends", "hide", "adjust", "objects", "parts" };
		// File types whose paths are made relative to the layer's folder
		const char* kFileTypes[] = { ".gltf", ".glb", ".png", ".jpg", ".ktx", ".tga", ".bin", ".json", ".wav", ".ogg",
			".mp3", ".lua", ".rml", ".rcss", ".bdae", ".obj", ".dae", ".fbx", ".mtl", ".dds", ".jpeg", ".ttf", ".otf", ".csv", ".txt" };

		bool read_json(const std::string& path, Json& out)
		{
			std::string text;
			if (!LevelDocument::read_file(path, text)) return false;   // the host's reader (a zip, the APK) or the disk
			out = Json::parse(text, nullptr, false);
			return !out.is_discarded();
		}

		bool is_file_path(const std::string& value)
		{
			if (value.empty() || value.size() > 512 || value.find("://") != std::string::npos || value.rfind("data:", 0) == 0) return false;
			if (value[0] == '/' || value[0] == '\\' || (value.size() > 1 && value[1] == ':')) return false;
			const size_t dot = value.find_last_of('.');
			if (dot == std::string::npos) return false;
			std::string ext = value.substr(dot);
			std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return std::find(std::begin(kFileTypes), std::end(kFileTypes), ext) != std::end(kFileTypes);
		}

		std::string rebase(const std::string& value, const fs::path& from, const fs::path& to)
		{
			const fs::path full = (from / value).lexically_normal();
			const fs::path relative = full.lexically_relative(to);
			return relative.empty() ? value : relative.generic_string();
		}

		// Every string that names a file, made relative to the layer's folder (the base's own folder when they match)
		void rebase_all(Json& node, const fs::path& from, const fs::path& to)
		{
			if (node.is_string())
			{
				const std::string value = node.get<std::string>();
				if (is_file_path(value)) node = rebase(value, from, to);
			}
			else if (node.is_object() || node.is_array())
			{
				for (auto& child : node) rebase_all(child, from, to);
			}
		}

		// How an adjusted object moved: about its old place, turned by 'turn' degrees (about Z, counterclockwise seen from
		// above, as an object's rot[2]) and scaled by 'scale', then carried to its new place
		struct Move
		{
			double x0 = 0, y0 = 0, x1 = 0, y1 = 0, turn = 0, scale = 1;
			bool identity() const { return x0 == x1 && y0 == y1 && turn == 0 && scale == 1; }
			void point(double& x, double& y) const
			{
				const double a = turn * 3.14159265358979 / 180.0, c = std::cos(a), s = std::sin(a);
				const double dx = (x - x0) * scale, dy = (y - y0) * scale;
				x = x1 + dx * c - dy * s;
				y = y1 + dx * s + dy * c;
			}
			// a collision item's "points" ([[x, y], ...])
			void apply_points(Json& item) const
			{
				if (!item.contains("points") || !item["points"].is_array()) return;
				for (auto& p : item["points"])
				{
					if (!p.is_array() || p.size() < 2) continue;
					double x = p[0].get<double>(), y = p[1].get<double>();
					point(x, y);
					p[0] = x;
					p[1] = y;
				}
			}
			// a wall or a destructible: its place. A wall's box turns with it ("angle"); one that ends square to the axes
			// has no angle, its sides swapped by an odd quarter turn
			void apply_place(Json& item) const
			{
				if (!item.contains("x") || !item.contains("y")) return;
				double x = item["x"].get<double>(), y = item["y"].get<double>();
				point(x, y);
				item["x"] = x;
				item["y"] = y;
				for (const char* size : { "w", "h", "d" })
					if (item.contains(size) && item[size].is_number()) item[size] = item[size].get<double>() * scale;
				if (!item.contains("w") || !item.contains("h") || turn == 0.0) return;
				const double was = item.contains("angle") && item["angle"].is_number() ? item["angle"].get<double>() : 0.0;
				double angle = std::fmod(was + turn, 360.0);
				if (angle < 0.0) angle += 360.0;
				const double quarters = angle / 90.0;
				if (std::fabs(quarters - std::round(quarters)) < 1e-3)
				{
					if (static_cast<int>(std::round(quarters)) % 2) std::swap(item["w"], item["h"]);
					item.erase("angle");
				}
				else
				{
					item["angle"] = angle;
				}
			}
		};

		double number(const Json& object, const char* key, double fallback)
		{
			return object.contains(key) && object[key].is_number() ? object[key].get<double>() : fallback;
		}

		double yaw_of(const Json& object)
		{
			return object.contains("rot") && object["rot"].is_array() && object["rot"].size() > 2 && object["rot"][2].is_number()
				? object["rot"][2].get<double>() : 0.0;
		}

		Move move_between(const Json& before, const Json& after)
		{
			Move m;
			m.x0 = number(before, "x", 0.0);
			m.y0 = number(before, "y", 0.0);
			m.x1 = number(after, "x", m.x0);
			m.y1 = number(after, "y", m.y0);
			m.turn = yaw_of(after) - yaw_of(before);
			const double s0 = number(before, "scale", 1.0);
			m.scale = s0 != 0.0 ? number(after, "scale", s0) / s0 : 1.0;
			return m;
		}

		// A folder without its trailing separator ("." for the working one), so paths relative to it come out right
		fs::path folder(const std::string& dir)
		{
			fs::path p = dir.empty() ? fs::path(".") : fs::path(dir).lexically_normal();
			if (p.has_parent_path() && p.filename().empty()) p = p.parent_path();
			return p;
		}
	}

	std::string level_base(const std::string& path)
	{
		Json root;
		if (!read_json(path, root) || !root.is_object()) return std::string();
		return root.value("extends", std::string());
	}

	Json compose_layer(const Json& base, const std::string& baseDir, const Json& layer, const std::string& layerDir,
		LevelLayering& layering)
	{
		const fs::path from = folder(baseDir), to = folder(layerDir);
		Json out = base;
		if (from != to) rebase_all(out, from, to);

		// what the layer hides: object keys, and game ids ("id:1004")
		std::set<std::string> keys;
		std::set<long long> ids;
		for (const auto& entry : layer.value("hide", Json::array()))
		{
			if (!entry.is_string()) continue;
			const std::string name = entry.get<std::string>();
			if (name.rfind("id:", 0) == 0)
			{
				try { ids.insert(std::stoll(name.substr(3))); }
				catch (...) {}
			}
			else
			{
				keys.insert(name);
			}
		}
		auto hidden_id = [&ids](const Json& item) { return item.is_object() && item.contains("id") && item["id"].is_number_integer() && ids.count(item["id"].get<long long>()) > 0; };

		// the base's objects: hidden ones out, adjusted ones patched; then the layer's
		Json objects = Json::array();
		const Json adjust = layer.value("adjust", Json::object());
		std::map<std::string, Move> moves;
		std::map<long long, Move> movesById;
		for (const auto& object : out.value("objects", Json::array()))
		{
			++layering.baseObjects;
			const std::string key = object.value("key", std::string());
			if ((!key.empty() && keys.count(key)) || hidden_id(object))
			{
				// a breakable one goes with its game id's walls and destructible too
				if (object.contains("id") && object["id"].is_number_integer() && object["id"].get<long long>() >= 0)
					ids.insert(object["id"].get<long long>());
				++layering.hidden;
				continue;
			}
			Json kept = object;
			if (!key.empty() && adjust.contains(key))
			{
				kept.merge_patch(adjust[key]);
				++layering.adjusted;
				// what it owns (collision, walls, its destructible) follows it
				const Move move = move_between(object, kept);
				if (!move.identity())
				{
					moves[key] = move;
					if (kept.contains("id") && kept["id"].is_number_integer() && kept["id"].get<long long>() >= 0)
						movesById[kept["id"].get<long long>()] = move;
				}
			}
			objects.push_back(std::move(kept));
		}
		for (const auto& object : layer.value("objects", Json::array()))
		{
			objects.push_back(object);
			++layering.added;
		}
		out["objects"] = std::move(objects);
		// what the layer names and the base does not have
		{
			std::set<std::string> present;
			for (const auto& object : base.value("objects", Json::array())) present.insert(object.value("key", std::string()));
			for (const auto& key : keys)
				if (!present.count(key)) layering.missing.push_back(key);
			for (auto it = adjust.begin(); it != adjust.end(); ++it)
				if (!present.count(it.key())) layering.missing.push_back(it.key());
		}

		// the collision the hidden objects own goes with them
		if (out.contains("collision") && out["collision"].is_object())
		{
			for (auto& section : out["collision"])
			{
				if (!section.is_array()) continue;
				Json kept = Json::array();
				for (const auto& item : section)
				{
					const bool owned = item.is_object() && keys.count(item.value("of", std::string()));
					if (owned || hidden_id(item)) continue;
					Json moved = item;
					if (item.is_object())
					{
						const auto by = moves.find(item.value("of", std::string()));
						if (by != moves.end()) by->second.apply_points(moved);
						else if (item.contains("id") && item["id"].is_number_integer())
						{
							const auto byId = movesById.find(item["id"].get<long long>());
							if (byId != movesById.end()) byId->second.apply_points(moved);
						}
					}
					kept.push_back(std::move(moved));
				}
				section = std::move(kept);
			}
		}
		for (const char* name : { "walls", "destructibles" })
		{
			if (!out.contains(name) || !out[name].is_array()) continue;
			Json kept = Json::array();
			for (const auto& item : out[name])
			{
				if (hidden_id(item)) continue;
				Json moved = item;
				if (item.is_object() && item.contains("id") && item["id"].is_number_integer())
				{
					const auto byId = movesById.find(item["id"].get<long long>());
					if (byId != movesById.end()) byId->second.apply_place(moved);
				}
				kept.push_back(std::move(moved));
			}
			out[name] = std::move(kept);
		}

		// the layer's sections over the base's
		for (auto it = layer.begin(); it != layer.end(); ++it)
		{
			const std::string& key = it.key();
			if (std::find_if(std::begin(kLayerOnly), std::end(kLayerOnly), [&key](const char* k) { return key == k; }) != std::end(kLayerOnly))
				continue;
			const bool added = std::find_if(std::begin(kAdded), std::end(kAdded), [&key](const char* k) { return key == k; }) != std::end(kAdded);
			if (added && out.contains(key) && out[key].is_array() && it.value().is_array())
			{
				for (const auto& item : it.value()) out[key].push_back(item);
			}
			else if (key == "collision" && out.contains(key) && out[key].is_object() && it.value().is_object())
			{
				for (auto part = it.value().begin(); part != it.value().end(); ++part)
				{
					Json& mine = out[key][part.key()];
					if (mine.is_array() && part.value().is_array())
						for (const auto& item : part.value()) mine.push_back(item);
					else
						mine = part.value();
				}
			}
			else if (out.contains(key) && out[key].is_object() && it.value().is_object() && key != "format")
			{
				out[key].merge_patch(it.value());
			}
			else
			{
				out[key] = it.value();
			}
		}
		out.erase("extends");
		out.erase("parts");
		// where the base is, from the layer's folder: what lives next to it (gi.json, audio.json) is found there
		Json& meta = out["meta"];
		if (!meta.is_object()) meta = Json::object();
		meta["layers"] = Json{ { "base", layering.base }, { "hidden", layering.hidden }, { "adjusted", layering.adjusted },
			{ "added", layering.added }, { "missing", layering.missing } };
		return out;
	}

	bool load_level(const std::string& path, LevelDocument& out, std::string& error, LevelLayering* layering)
	{
		Json root;
		if (!read_json(path, root) || !root.is_object())
		{
			// not readable here: LevelDocument::load says why
			return out.load(path, error);
		}
		if (!root.contains("extends") || !root["extends"].is_string())
		{
			return out.load(path, error);
		}
		const std::string layerDir = LevelDocument::directory_of(path);
		LevelLayering info;
		info.base = root["extends"].get<std::string>();
		const std::string basePath = (folder(layerDir) / info.base).lexically_normal().string();
		info.baseDir = LevelDocument::directory_of(basePath);
		if (basePath == fs::path(path).lexically_normal().string())
		{
			error = "'" + path + "' extends itself";
			return false;
		}
		LevelDocument base;
		LevelLayering below;
		if (!load_level(basePath, base, error, &below))
		{
			error = "the base of '" + path + "': " + error;
			return false;
		}
		// the base's base (when it extends another): what lives next to the base is still in the first one
		if (!below.base.empty())
		{
			info.baseDir = below.baseDir;
		}
		base.parts.clear();
		Json composed = compose_layer(base.to_json(), LevelDocument::directory_of(basePath), root, layerDir, info);
		if (!below.base.empty())
		{
			composed["meta"]["layers"]["base"] = rebase(below.base, folder(LevelDocument::directory_of(basePath)), folder(layerDir));
		}
		if (!out.from_json(composed, error))
		{
			error = "'" + path + "' over its base: " + error;
			return false;
		}
		if (layering) *layering = info;
		return true;
	}
}
