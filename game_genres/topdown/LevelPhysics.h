///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelPhysics.h
// Purpose: A level's physics from its document (LevelDocument): the walls of its props (rectangles and
//          circles, owned by the prop's actor so bullets can break it), its collision polygons ("solid"
//          blocks everyone, "player" only the player; sand and water do not block), and the walkable
//          grid over all of it, on the game's collision layers.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/TopDownGame.h"

#include <cstdint>
#include <functional>

namespace Pakal
{
	class LevelDocument;
}

namespace Pakal::TopDown
{
	struct LevelPhysicsCount
	{
		size_t walls = 0;      // props' walls
		size_t outlines = 0;   // collision polygons
	};

	// 'wallOwner' gives the actor of a prop by its game id (0: none); 'isBarricade' says whether an actor is a barricade
	// (it stops bodies only). 'agentRadius' is how wide the bodies that walk the grid are.
	LevelPhysicsCount build_level_physics(const Pakal::LevelDocument& doc, Pakal::PhysicsWorld2D& world, Pakal::NavGrid2D& nav,
		const TopDownLayers& layers, float agentRadius, const std::function<uintptr_t(int gameId)>& wallOwner,
		const std::function<bool(uintptr_t actor)>& isBarricade);
}
