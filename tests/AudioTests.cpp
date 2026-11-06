///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: AudioTests.cpp
//
// Purpose: Checks the spatial audio math used by the sound backends: distance gain and left/right
//          panning for the listener's orientation. And the music mixer on a fixture (tests/fixtures/music_tone.ogg,
//          two seconds of 440 Hz on the left and 660 Hz on the right): its loop, its stereo, its fade out. No audio
//          device is needed.
//
// Exit code 0 when every check passes.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "SpatialAudio.h"
#include "SoundManager.h"
#include "MusicMixer.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

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

	// The music mixer
	{
		const int rate = 48000;
		std::ifstream file(PAKAL_TEST_FIXTURES "/music_tone.ogg", std::ios::binary);
		auto ogg = std::make_shared<std::vector<uint8_t>>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
		MusicMixer mixer;
		check(!mixer.play(std::make_shared<std::vector<uint8_t>>(100, uint8_t(7)), MusicSettings()), "music: bytes that are not OGG do not play");
		MusicSettings settings;
		settings.gain = 1.0f;
		settings.fadeSeconds = 0.0f;
		settings.loopFadeSeconds = 0.5f;
		check(mixer.play(ogg, settings), "music: the fixture plays");
		// 5 s: two seams of its loop. The quietest tenth of a second on each side, after the first 0.1 s
		const int window = rate / 10;
		float quietest[2] = { 1.0f, 1.0f };
		double crossing[2] = { 0.0, 0.0 };
		std::vector<float> block(2 * window);
		for (int w = 0; w < 50; ++w)
		{
			std::fill(block.begin(), block.end(), 0.0f);
			mixer.render(block.data(), window, rate);
			double energy[2] = { 0.0, 0.0 };
			for (int i = 0; i < window; ++i)
			{
				for (int c = 0; c < 2; ++c)
				{
					energy[c] += block[2 * i + c] * block[2 * i + c];
					if (i > 0 && (block[2 * (i - 1) + c] < 0.0f) != (block[2 * i + c] < 0.0f)) crossing[c] += 1.0;
				}
			}
			for (int c = 0; c < 2 && w > 0; ++c) quietest[c] = std::min(quietest[c], static_cast<float>(std::sqrt(energy[c] / window)));
		}
		check(quietest[0] > 0.15f && quietest[1] > 0.15f, "music: the loop plays on with no gap at its seams");
		check(crossing[1] > crossing[0] * 1.3, "music: stereo kept (the right side's tone is higher)");
		mixer.stop(0.5f);
		for (int w = 0; w < 6; ++w)
		{
			std::fill(block.begin(), block.end(), 0.0f);
			mixer.render(block.data(), window, rate);
		}
		std::fill(block.begin(), block.end(), 0.0f);
		mixer.render(block.data(), window, rate);
		float loudest = 0.0f;
		for (float v : block) loudest = std::max(loudest, std::fabs(v));
		check(loudest < 1e-4f, "music: silent once its fade out is over");
	}

	std::printf("%s (%d failures)\n", g_failures == 0 ? "ALL PASSED" : "FAILED", g_failures);
	std::fflush(stdout);
	return g_failures == 0 ? 0 : 1;
}
