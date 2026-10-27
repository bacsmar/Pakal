///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelLook.cpp
// Purpose: How a Zombies level looks - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "LevelLook.h"

#include <cstdlib>
#include <string>

using Pakal::TopDown::CharacterViewComponent;

namespace Zombies
{
	namespace
	{
		// Role models are fitted to a height in meters: the converted characters do not carry a reliable unit
		const float kPlayerSize = 1.8f;
		const float kEnemySize = 1.8f;

		void on_off(const char* name, bool& value)
		{
			if (const char* env = std::getenv(name))
			{
				if (std::string(env) == "off") value = false;
				else if (std::string(env) == "on") value = true;
			}
		}
	}

	LevelLook LevelLook::read(const nlohmann::json& lighting, const nlohmann::json& postJson)
	{
		LevelLook look;
		static_cast<Pakal::TopDown::SceneLook&>(look) = Pakal::TopDown::SceneLook::from_json(lighting, postJson);
		// the plastic toy look (a tight highlight of the sun): ZOMBIES_GLOSS=<strength> to compare
		if (const char* env = std::getenv("ZOMBIES_GLOSS")) look.gloss = static_cast<float>(std::atof(env));
		// sun shadows: ZOMBIES_SHADOW_SIZE (the map's texels), ZOMBIES_SHADOWS=off
		if (const char* env = std::getenv("ZOMBIES_SHADOW_SIZE")) look.shadows.mapSize = std::atoi(env);
		on_off("ZOMBIES_SHADOWS", look.shadows.enabled);
		// the picture: ZOMBIES_AO=<strength> and ZOMBIES_TILT=<blur pixels> replace the level's values, ZOMBIES_POST=off
		if (const char* env = std::getenv("ZOMBIES_AO")) look.post.aoStrength = static_cast<float>(std::atof(env));
		if (const char* env = std::getenv("ZOMBIES_TILT")) look.post.tiltBlur = static_cast<float>(std::atof(env));
		if (const char* env = std::getenv("ZOMBIES_POST_QUALITY")) look.post.quality = static_cast<float>(std::atof(env)); // 1: all the samples
		on_off("ZOMBIES_POST", look.post.enabled);
		return look;
	}

	CharacterViewComponent::Look default_player_look()
	{
		CharacterViewComponent::Look look;
		// Arms at rest while not firing: the fire clip raises the gun over them (Clips::fire)
		look.clips.idle = { "Idle_Loop", "Idle", "Idle_Gun", "idle_glock", "idle" };
		look.clips.move = { "Run", "Run_Gun", "run_glock", "run_base", "walk_01" };
		look.clips.attack = { "Idle_Gun_Shoot", "Idle_Gun", "firing_dualglocks", "idle_glock", "Idle" };
		look.clips.death = { "Death", "death", "knee_defeated" };
		look.clips.fast = { "Jog_Fwd_Loop", "Run", "Run_Gun", "run_glock" };
		look.clips.hit = { "HitReact" };
		look.clips.gunIdle = { "Idle_Gun" };   // a two-handed gun at rest: in both hands
		look.clips.fire = { "Pistol_Shoot" };
		look.clips.reload = { "Pistol_Reload" };
		look.clips.walkFire = { "Walk_Gun" };
		look.clips.meleeIdle = { "Idle" };
		look.clips.meleeMove = { "Run" };
		look.clips.meleeMoving = { "Run_Slash" };
		look.height = kPlayerSize;
		return look;
	}

	CharacterViewComponent::Look default_enemy_look()
	{
		CharacterViewComponent::Look look;
		look.clips.idle = { "Idle", "idle" };
		look.clips.move = { "Walk", "Run", "walk_01", "walk_02" };
		look.clips.attack = { "Punch", "Idle_Attack", "attack_l", "attack_r", "attack_bite" };
		look.clips.death = { "Death", "death", "death_melee" };
		look.clips.fast = { "Run_Arms", "Run", "walk_02_fast", "Walk" };
		look.clips.hit = { "HitReact" };
		look.height = kEnemySize;
		return look;
	}
}
