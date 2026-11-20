///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: TimeScale.h
// Purpose: How fast a game's simulation runs against real time: 1 normal, 0 stopped, up to kMaxTimeScale
//          faster. SimulationClock multiplies the frame's seconds by it, so every game on fixed steps (the
//          TopDown genre's LevelRuntime, the Zombies level) follows it, its animations and effects too. The
//          engine's QA tools set it (time.scale); a game never needs to.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Config.h"

namespace Pakal
{
	constexpr float kMaxTimeScale = 4.0f;

	// 1 unless the QA tools changed it
	_PAKALExport float sim_time_scale();
	// Clamped to 0..kMaxTimeScale
	_PAKALExport void set_sim_time_scale(float scale);
}
