///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SimulationClockTests.cpp
// Purpose: The fixed-step clock: the same steps whatever the frames, alpha between steps, long frames
//          cut, paused time dropped.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "SimulationClock.h"

#include <cmath>
#include <cstdio>

int main()
{
	int failures = 0;
	auto check = [&](bool ok, const char* message)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
		if (!ok) ++failures;
	};
	// Steps and frames that are exact in binary, so the counts do not depend on rounding
	const float step = 1.0f / 64.0f;

	{
		Pakal::SimulationClock fast(step), slow(step), uneven(step);
		int a = 0, b = 0, c = 0;
		for (int i = 0; i < 128; ++i) a += fast.advance(step);          // 128 frames of one step
		for (int i = 0; i < 32; ++i) b += slow.advance(4.0f * step);    // 32 frames of four steps
		for (int i = 0; i < 128; ++i) c += uneven.advance(i % 2 ? 0.5f * step : 1.5f * step);
		check(a == 128 && b == 128 && c == 128, "two seconds give 128 steps whatever the frames");
		check(fast.steps() == 128, "steps() counts the steps run");
	}
	{
		Pakal::SimulationClock clock(step);
		const int steps = clock.advance(2.5f * step);
		check(steps == 2 && std::fabs(clock.alpha() - 0.5f) < 1e-5f, "a frame between steps is drawn halfway (alpha 0.5)");
		check(clock.advance(0.0f) == 0 && std::fabs(clock.alpha() - 0.5f) < 1e-5f, "a frame with no time runs no step");
	}
	{
		Pakal::SimulationClock clock(step, 0.25f);
		check(clock.advance(3.0f) == 16, "a long frame (a stall) is cut to maxFrame: 0.25 s, 16 steps");
	}
	{
		Pakal::SimulationClock clock(step);
		clock.advance(0.75f * step);
		clock.hold();
		check(clock.advance(0.5f * step) == 0 && std::fabs(clock.alpha() - 0.5f) < 1e-5f, "paused time is dropped, not caught up");
	}
	{
		Pakal::SimulationClock clock(step);
		check(clock.frame_seconds() == 0.0f, "the first frame brings no time");
	}
	std::printf("%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
	return failures == 0 ? 0 : 1;
}
