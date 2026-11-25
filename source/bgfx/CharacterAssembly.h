///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CharacterAssembly.h
// Purpose: A character made of parts (CharacterRecipe, docs/MODULAR_CHARACTERS.md) assembled into one Assimp scene:
//          the skeleton file's bones and clips, every part's meshes skinned to them (by bone names), the recipe's blend
//          shapes baked into the vertices, the bones with no weight left out and the folded ones merged into their
//          parent, and the parts' materials and embedded textures that are the same kept once (so parts painted from
//          one atlas draw in one call). MeshComponent3D_Bgfx::load_character loads the scene as it loads a file.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "CharacterRecipe.h"

#include <memory>
#include <string>

struct aiScene;

namespace Pakal
{
	struct AssembledCharacter
	{
		std::unique_ptr<aiScene> scene;
		std::string label;     // the path the scene stands for: the first part's (its folder is where textures are found)
		bool flipV = false;    // glTF parts: their V is flipped, as a glTF file's
		std::string report;    // what was assembled, for the log
		AssembledCharacter();
		~AssembledCharacter();
		AssembledCharacter(AssembledCharacter&&) noexcept;
		AssembledCharacter& operator=(AssembledCharacter&&) noexcept;
	};

	// False, with 'error' set, when a file cannot be read, a part shares no bone with the skeleton, or the bones the
	// parts use are more than 'maxBones' (the skinning limit) after folding
	bool assemble_character(const CharacterRecipe& recipe, AssembledCharacter& out, std::string& error, size_t maxBones = 64);
}
