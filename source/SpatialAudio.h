///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SpatialAudio.h
//
// Purpose: Pure spatial audio math shared by the audio backends and tests. No audio device needed.
//          Coordinates are the engine's world coordinates. The default listener looks down -Z with
//          +Y up, so its right side is +X.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "PakalMath.h"

#include <algorithm>
#include <cmath>

namespace Pakal
{
	namespace SpatialAudio
	{
		// Inverse-distance model used by OpenAL, and therefore by SFML: gain is 1 inside minDistance
		// and falls off with the attenuation factor beyond it. Attenuation 0 means no falloff.
		inline float inverse_distance_gain(float distance, float minDistance, float attenuation)
		{
			if (attenuation <= 0.0f || minDistance <= 0.0f)
			{
				return 1.0f;
			}
			const float clamped = std::max(distance, minDistance);
			return minDistance / (minDistance + attenuation * (clamped - minDistance));
		}

		// Lateral position of a source as the listener hears it: -1 is fully left, +1 fully right.
		inline float pan_for_listener(const tmath::vector3df& listenerPosition,
			const tmath::vector3df& listenerForward,
			const tmath::vector3df& listenerUp,
			const tmath::vector3df& sourcePosition)
		{
			// right = forward x up
			const float rx = listenerForward.y * listenerUp.z - listenerForward.z * listenerUp.y;
			const float ry = listenerForward.z * listenerUp.x - listenerForward.x * listenerUp.z;
			const float rz = listenerForward.x * listenerUp.y - listenerForward.y * listenerUp.x;
			const float rightLength = std::sqrt(rx * rx + ry * ry + rz * rz);
			if (rightLength <= 0.0f)
			{
				return 0.0f;
			}

			const float dx = sourcePosition.x - listenerPosition.x;
			const float dy = sourcePosition.y - listenerPosition.y;
			const float dz = sourcePosition.z - listenerPosition.z;
			const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
			if (distance <= 0.0f)
			{
				return 0.0f;
			}

			const float lateral = (dx * rx + dy * ry + dz * rz) / (distance * rightLength);
			return std::clamp(lateral, -1.0f, 1.0f);
		}
	}
}
