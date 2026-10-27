///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: AudioTests.cpp
//
// Purpose: Checks the spatial audio math used by the sound backends: distance gain and left/right
//          panning for the listener's orientation. No audio device is needed.
//
// Exit code 0 when every check passes.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "SpatialAudio.h"
#include "SoundManager.h"

#include <cmath>
#include <cstdio>

using namespace Pakal;
using vec3 = tmath::vector3df;

namespace
{
	int g_failures = 0;

	void check(bool condition, const char* description)
	{
		std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", description);
		if (!condition)
		{
			++g_failures;
		}
	}

	bool near(float a, float b, float tolerance = 1e-3f)
	{
		return std::fabs(a - b) <= tolerance;
	}
}

int main()
{
	// Distance gain (inverse distance model, as in OpenAL and SFML)
	check(near(SpatialAudio::inverse_distance_gain(0.5f, 1.0f, 1.0f), 1.0f), "gain is 1 inside min distance");
	check(near(SpatialAudio::inverse_distance_gain(1.0f, 1.0f, 1.0f), 1.0f), "gain is 1 at min distance");
	check(near(SpatialAudio::inverse_distance_gain(2.0f, 1.0f, 1.0f), 0.5f), "gain is 0.5 at twice min distance");
	check(near(SpatialAudio::inverse_distance_gain(10.0f, 1.0f, 1.0f), 1.0f / 10.0f), "gain is 1/10 at ten times min distance");
	check(near(SpatialAudio::inverse_distance_gain(50.0f, 1.0f, 0.0f), 1.0f), "zero attenuation disables falloff");

	bool decreasing = true;
	float previous = 1.0f;
	for (float d = 1.0f; d <= 100.0f; d += 1.0f)
	{
		const float g = SpatialAudio::inverse_distance_gain(d, 1.0f, 1.0f);
		decreasing = decreasing && g <= previous + 1e-6f;
		previous = g;
	}
	check(decreasing, "gain never increases with distance");

	// Panning for the default listener: looking down -Z, up +Y, so right is +X
	const vec3 origin(0.0f, 0.0f, 0.0f);
	const vec3 forward(0.0f, 0.0f, -1.0f);
	const vec3 up(0.0f, 1.0f, 0.0f);

	check(near(SpatialAudio::pan_for_listener(origin, forward, up, vec3(5.0f, 0.0f, 0.0f)), 1.0f),
		"source on the listener's right pans fully right");
	check(near(SpatialAudio::pan_for_listener(origin, forward, up, vec3(-5.0f, 0.0f, 0.0f)), -1.0f),
		"source on the listener's left pans fully left");
	check(near(SpatialAudio::pan_for_listener(origin, forward, up, vec3(0.0f, 0.0f, -5.0f)), 0.0f),
		"source straight ahead is centered");
	check(near(SpatialAudio::pan_for_listener(origin, forward, up, vec3(0.0f, 0.0f, 5.0f)), 0.0f),
		"source straight behind is centered");
	check(near(SpatialAudio::pan_for_listener(origin, forward, up, vec3(5.0f, 0.0f, -5.0f)), std::sqrt(0.5f), 1e-3f),
		"source 45 degrees to the right pans 0.707");
	check(near(SpatialAudio::pan_for_listener(origin, forward, up, origin), 0.0f),
		"source at the listener position is centered");

	// Listener turned to look along +X: its right side is +Z
	const vec3 turnedForward(1.0f, 0.0f, 0.0f);
	check(near(SpatialAudio::pan_for_listener(origin, turnedForward, up, vec3(0.0f, 0.0f, 5.0f)), 1.0f),
		"after turning to +X, a source on +Z is on the right");
	check(near(SpatialAudio::pan_for_listener(origin, turnedForward, up, vec3(0.0f, 0.0f, -5.0f)), -1.0f),
		"after turning to +X, a source on -Z is on the left");

	// Listener position is taken into account
	check(near(SpatialAudio::pan_for_listener(vec3(100.0f, 0.0f, 0.0f), forward, up, vec3(105.0f, 0.0f, 0.0f)), 1.0f),
		"pan is relative to the listener position");

	// Defaults used by SoundSettings match the SFML defaults
	SoundSettings settings;
	check(near(settings.min_distance, 1.0f) && near(settings.attenuation, 1.0f),
		"SoundSettings defaults: min distance 1, attenuation 1");

	std::printf("%s (%d failures)\n", g_failures == 0 ? "ALL PASSED" : "FAILED", g_failures);
	std::fflush(stdout);
	return g_failures == 0 ? 0 : 1;
}
