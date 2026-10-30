///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelLayers.h
// Purpose: A level made of a base and a layer over it. A mission's level.json says
//          "extends": "<the base level.json, relative to it>" and holds only what the mission
//          changes; loading it composes the two into one document:
//          - the base's objects, minus the ones its "hide" names, changed by its "adjust", and then the
//            layer's own objects;
//          - collision, zones, walls, enemies, pickups and destructibles: the base's and the layer's together;
//          - every other section (rules, script, spawners, lighting, post, visuals...): the layer's over the base's,
//            objects merged key by key (a JSON merge patch), anything else replaced.
//          The base's file paths (models, textures, sounds) are made relative to the layer's folder. A base may
//          extend another base. The composed document says where its base is in meta.layers.base, so what sits
//          next to the base (its baked light, its sounds) is found there.
//
//          "hide": ["<object key>", "id:<game id>"]: the object, with the collision it owns (a collision
//                  segment's "of" is the key of the object it belongs to; a wall, a destructible and an object
//                  with that game id go with "id:")
//          "adjust": { "<object key>": { "x": 120, "rot": [0, 0, 90] } }: a merge patch on the object. What it owns
//                  (collision segments whose "of" is its key; walls and a destructible with its game id) moves,
//                  turns and scales with it
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "LevelDocument.h"

#include <string>
#include <vector>

namespace Pakal
{
	struct LevelLayering
	{
		std::string base;     // the base level's file, as the layer names it ("" when the level extends none)
		std::string baseDir;  // its folder, with a trailing separator, as a path from the working folder
		size_t baseObjects = 0;
		size_t hidden = 0;
		size_t adjusted = 0;
		size_t added = 0;
		// keys "hide" or "adjust" name that the base does not have (a base made again with other keys): not an error, but
		// the layer no longer does what it says
		std::vector<std::string> missing;
	};

	// Reads a level; when it extends a base, the composition of both. 'layering' (optional) tells what was composed.
	bool load_level(const std::string& path, LevelDocument& out, std::string& error, LevelLayering* layering = nullptr);

	// The base a level file extends, as written in it ("" when none, or the file cannot be read)
	std::string level_base(const std::string& path);

	// The composition itself: 'base' (a document's JSON, its paths relative to 'baseDir') under 'layer' (a layer file's
	// JSON, in 'layerDir'). Folders end with a separator or are empty.
	Json compose_layer(const Json& base, const std::string& baseDir, const Json& layer, const std::string& layerDir,
		LevelLayering& layering);
}
