///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: AutoplayBot.h
// Purpose: A simple player for unattended checks (ZOMBIES_AUTOPLAY): it shoots the nearest zombie it
//          can see, keeps its distance, opens the gift box and picks up money. ZOMBIES_AUTOPLAY=idle
//          stands still. It reads only the simulation, so its runs are as repeatable as the game.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "TopDown.h"
#include "topdown/TopDownGame.h"

#include <cstdint>

namespace Zombies
{
	class ZombiesGame;

	// The bot's input for this step
	PlayerInput autoplay_input(ZombiesGame& game, uintptr_t playerId);
}
