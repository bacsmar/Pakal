///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelCameraPath.h
// Purpose: A level's camera path ("camera": {"<name>": {"seconds", "end", "shots": [{"eye", "look", "at", "fov"}]}})
//          as a CameraPan's keys, in render space (meters: level x, z up, -y). The level's units are its own
//          (unitsPerMeter); "look" is a point; "at" (a share of the length) is optional, the shots spread evenly
//          without it; "fov" (vertical degrees) is optional, the play camera's without it; "end": "play" (the
//          default) leaves room for the play camera as the last key, at 1, which the caller adds (it knows
//          where the game's camera rests).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/CameraPan.h"

#include <algorithm>
#include <string>

namespace Pakal::TopDown
{
	struct LevelCameraPath
	{
		std::vector<CameraPan::Key> keys;   // the shots, without the play camera
		float seconds = 0.0f;               // 0: the caller's length
		bool endOnPlay = true;              // the caller adds the play camera's key at 1

		// A level point (x, y, z up; level units) in render space (meters)
		static CameraPan::Point to_render(double x, double y, double z, double unitsPerMeter)
		{
			const double u = unitsPerMeter > 0.0 ? 1.0 / unitsPerMeter : 1.0;
			return { static_cast<float>(x * u), static_cast<float>(z * u), static_cast<float>(-y * u) };
		}

		// 'path' is one of the level's camera paths (an object with "shots"); false when it has none
		template <typename JsonT>
		bool read(const JsonT& path, double unitsPerMeter, float playFovY)
		{
			keys.clear();
			if (!path.is_object() || !path.contains("shots") || !path["shots"].is_array() || path["shots"].empty()) return false;
			const JsonT& shots = path["shots"];
			seconds = path.contains("seconds") && path["seconds"].is_number() ? path["seconds"].template get<float>() : 0.0f;
			endOnPlay = !path.contains("end") || !path["end"].is_string() || path["end"].template get<std::string>() == "play";
			const size_t count = shots.size() + (endOnPlay ? 1 : 0);
			auto point = [unitsPerMeter](const JsonT& p) -> CameraPan::Point
			{
				if (!p.is_array() || p.size() < 3) return { 0.0f, 0.0f, 0.0f };
				return to_render(p[0].template get<double>(), p[1].template get<double>(), p[2].template get<double>(), unitsPerMeter);
			};
			for (size_t i = 0; i < shots.size(); ++i)
			{
				const JsonT& shot = shots[i];
				CameraPan::Key key;
				key.eye = point(shot.contains("eye") ? shot["eye"] : JsonT());
				key.target = point(shot.contains("look") ? shot["look"] : JsonT());
				key.fovY = shot.contains("fov") && shot["fov"].is_number() ? shot["fov"].template get<float>() : playFovY;
				const float even = count > 1 ? static_cast<float>(i) / static_cast<float>(count - 1) : 0.0f;
				key.at = shot.contains("at") && shot["at"].is_number() ? shot["at"].template get<float>() : even;
				if (endOnPlay) key.at = std::min(key.at, 0.98f);   // the play camera is the last, at 1
				keys.push_back(key);
			}
			return true;
		}
	};
}
