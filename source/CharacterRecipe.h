///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CharacterRecipe.h
// Purpose: A character made of parts (docs/MODULAR_CHARACTERS.md): the skeleton, the part in each slot, the body shapes
//          baked in. The graphics backend assembles it into one skinned model drawn in one call (the parts share the
//          skeleton by bone names, their materials by texture). Plain data: no backend; a game builds it in its
//          customization menu, saves it with the profile, sends it over the network.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <map>
#include <string>
#include <vector>

namespace Pakal
{
	struct CharacterRecipe
	{
		// The file whose bones (their hierarchy and rest pose) and animation clips the character takes: a part that has them
		// all, or the rig alone. Empty: the first part's.
		std::string skeleton;
		// One part per slot ("body", "head", "hair", "torso", "legs", "feet", a piece of gear...): a model skinned to the same
		// skeleton (bone names). 'mesh' takes only the meshes of that name from the file (a file with every variant in it);
		// empty, all of them. A part that is not skinned (a hat, a rigid shield) follows the bone named like the node it
		// hangs from in its own file.
		struct Part
		{
			std::string slot;
			std::string file;
			std::string mesh;
		};
		std::vector<Part> parts;
		// Blend shapes baked in when the character is assembled, by name, 0..1 (the body's build: "masculineFeminine",
		// "defaultHeavy"...; a face's expression). A part without one of them is left as it is. Nothing of them is left at
		// run time: the assembled model has no blend shapes.
		std::map<std::string, float> shapes;
		// Bones folded into their parent: their weights go to the nearest bone above them not folded (by name prefix:
		// "hair_dyn_" folds every dynamic hair bone). For a rig over the skinning limit (64 bones): a hair that moves with
		// the head reads the same from a top-down camera.
		std::vector<std::string> foldBones;

		// From JSON text: {"skeleton": "...", "parts": [{"slot": "body", "file": "...", "mesh": "..."}], "shapes": {"name": 0.5},
		// "foldBones": ["hair_dyn_"]}. Relative files are taken from 'baseDir' (the recipe's folder; empty: as written).
		// False, with 'error' set, when the text is not a recipe.
		bool from_json(const std::string& text, const std::string& baseDir, std::string& error);
		std::string to_json() const;
		// A part's file by its slot ("" when the slot is empty)
		const Part* part(const std::string& slot) const;
		void set_part(const std::string& slot, const std::string& file, const std::string& mesh = std::string());
	};
}
