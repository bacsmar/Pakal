///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelLayerEditing.h
// Purpose: Editing a layer over a base (LevelLayers) as one level, and saving only the layer. The
//          level is edited composed with its base; saving compares it with the base as the layer
//          sees it, and writes:
//          - the objects the base does not have, as the layer's own "objects";
//          - the base's objects that are gone, as "hide" (by key; the layer's "id:" entries are kept);
//          - the base's objects that changed, as "adjust" (the smallest merge patch on each one);
//          - the zones the base does not have, as the layer's "zones";
//          - every other section of the layer file as it was.
//          A base whose objects have no keys cannot be layered over (a layer names them by key):
//          give them keys first (pakal-level <base> keys).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "LevelDocument.h"
#include "LevelLayers.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace Pakal
{
	class LevelLayerEditing
	{
	public:
		// Reads a level for editing: composed with its base when it is a layer. False with an error when it cannot be read,
		// or its base has objects without keys.
		bool open(const std::string& path, LevelDocument& composed, std::string& error);
		bool layered() const { return m_layered; }
		const LevelLayering& layering() const { return m_layering; }
		// The base's file, as a path from the working folder ("" when not layered)
		const std::string& base_path() const { return m_basePath; }

		// The object is the base's (as the layer sees it: possibly adjusted, or hidden)
		bool is_base(const std::string& key) const { return m_reference.count(key) > 0; }
		// The base's objects the edited level does not have (the ones the layer hides), in the base's order
		std::vector<std::string> hidden(const LevelDocument& current) const;
		// A base object as the base has it (its paths from the layer's folder), to put back one that was hidden; null when none
		Json base_object(const std::string& key) const;
		// Where a base object stands among the base's objects that are shown
		int base_index(const std::string& key, const LevelDocument& current) const;

		// The layer file's JSON for the edited level
		Json layer_json(const LevelDocument& current) const;
		// Writes the layer file
		bool save(const std::string& path, const LevelDocument& current, std::string& error) const;

		// A new, empty layer over 'basePath' at 'layerPath' (its folders made when they do not exist)
		static bool create(const std::string& layerPath, const std::string& basePath, std::string& error);

	private:
		bool m_layered = false;
		LevelLayering m_layering;
		std::string m_basePath;
		Json m_raw;                                // the layer file as read
		std::vector<std::string> m_order;          // the base's object keys, in its order
		std::map<std::string, Json> m_reference;   // the base's objects by key, as the layer sees them (not hidden, not adjusted)
		std::set<std::string> m_baseZones;
		Json m_baseSpawn;
		Json m_baseCamera = Json::object();   // the base's camera paths ("camera"), by name
	};
}
