///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: WorldOverlays.h
// Purpose: What Zombies draws over the level each frame, in the world: zombies' health bars and the
//          coins they carry, the escorted survivor's bar and ring, the barricades and the thing to
//          defend, pickups as gems and crosses, the gift box, the script's beacons and the arrow to the
//          next one, the aiming cursor, and the colliders (F3). It only reads the game.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "TopDown.h"
#include "topdown/view/CameraRig.h"
#include "topdown/view/InputMapper.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace Pakal
{
	class BgfxGraphicsSystem;
	class PhysicsWorld2D;
}

namespace Zombies
{
	class GameRules;
	class ZombiesGame;

	struct OverlayScene
	{
		Pakal::BgfxGraphicsSystem& graphics;
		ZombiesGame& game;
		const GameRules& rules;
		const CameraRig& camera;
		const InputMapper& controls;
		Pakal::PhysicsWorld2D& world;
		const std::vector<int>& markers;              // the script's beacons (ShowMarker)
		uintptr_t playerId = 0;
		float alpha = 1.0f;                            // how far the frame is between two steps
		std::function<bool(int id, b2Vec2& at)> pointPosition; // a named point of the level
		bool showColliders = false;
	};

	void draw_world_overlays(const OverlayScene& scene);
}
