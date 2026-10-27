///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelLook.h
// Purpose: How a Zombies level looks: TopDown's SceneLook of the level's "lighting" and "post"
//          sections, with the ZOMBIES_* debug variables over it; and how its characters look when the
//          level says nothing. The level and the character viewer use them.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/view/CharacterViewComponent.h"
#include "topdown/view/SceneLook.h"

#include <nlohmann/json.hpp>

namespace Zombies
{
	struct LevelLook : Pakal::TopDown::SceneLook
	{
		// The level's sections, and the debug variables over them: ZOMBIES_GLOSS, ZOMBIES_SHADOWS=on|off,
		// ZOMBIES_SHADOW_SIZE, ZOMBIES_POST=on|off, ZOMBIES_AO, ZOMBIES_TILT, ZOMBIES_POST_QUALITY
		static LevelLook read(const nlohmann::json& lighting, const nlohmann::json& post);
	};

	// How the characters look and move when the level says nothing else. Clip names are in order of preference (the first
	// one the model has wins): the open-source kit's (Quaternius, CC0) first, then the original game's.
	Pakal::TopDown::CharacterViewComponent::Look default_player_look();
	Pakal::TopDown::CharacterViewComponent::Look default_enemy_look();
}
