///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: TimeScale.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "TimeScale.h"

#include <algorithm>
#include <atomic>

namespace Pakal
{
	namespace
	{
		std::atomic<float> s_scale{ 1.0f };
	}

	float sim_time_scale()
	{
		return s_scale.load(std::memory_order_relaxed);
	}

	void set_sim_time_scale(float scale)
	{
		s_scale.store(std::clamp(scale, 0.0f, kMaxTimeScale));
	}
}
