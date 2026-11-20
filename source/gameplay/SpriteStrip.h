///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SpriteStrip.h
//
// Purpose: What converted game data say about a sprite and a state, shared by the genres that play them (Versus,
//          Platformer): an animation as a strip of equal cells (frames side by side: what SpriteComponent2D draws with
//          set_frame_size / set_frame), and what a state does on entering it (the converted script's function). Header only.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace Pakal
{
	struct SpriteStrip
	{
		std::string file;
		int frames = 1, frameWidth = 1, frameHeight = 1;   // pixels
		std::vector<int> ms;                               // of each frame
		bool loop = false;

		int frame_ms(int frame) const { return frame >= 0 && frame < static_cast<int>(ms.size()) ? ms[frame] : 60; }
		int total_ms() const
		{
			int total = 0;
			for (int i = 0; i < frames; ++i) total += frame_ms(i);
			return std::max(1, total);
		}
		// The frame showing 'elapsedMs' after the animation started (a loop goes round, a strip that plays once holds its last)
		int frame_at(double elapsedMs) const
		{
			double t = std::max(0.0, elapsedMs);
			if (loop) t = t - static_cast<double>(total_ms()) * static_cast<double>(static_cast<long long>(t / total_ms()));
			for (int i = 0; i < frames; ++i)
			{
				if (t < frame_ms(i)) return i;
				t -= frame_ms(i);
			}
			return frames - 1;
		}

		static SpriteStrip from_json(const nlohmann::json& j)
		{
			SpriteStrip s;
			s.file = j.value("file", std::string());
			s.frames = std::max(1, j.value("frames", 1));
			s.frameWidth = std::max(1, j.value("frameWidth", 1));
			s.frameHeight = std::max(1, j.value("frameHeight", 1));
			s.ms = j.value("ms", std::vector<int>());
			s.loop = j.value("loop", false);
			return s;
		}
	};

	struct StateAction
	{
		std::vector<std::string> anim;   // one of them, at random
		std::string sound;
		bool setVelocityX = false, setVelocityY = false;
		float velocityX = 0.0f, velocityY = 0.0f;
		bool impulse = false;
		float impulseX = 0.0f, impulseY = 0.0f;
		bool damage = false;             // a hit box for as long as the animation
		int timerMs = 0;                 // is_timer_expired holds after this long in the state
		int flip = -1;                   // the sprite flipped (1) or not (0), -1 as it is

		static StateAction from_json(const nlohmann::json& j)
		{
			StateAction a;
			a.anim = j.value("anim", std::vector<std::string>());
			a.sound = j.value("sound", std::string());
			if (j.contains("velocity") && j["velocity"].is_array() && j["velocity"].size() == 2)
			{
				const auto& v = j["velocity"];
				a.setVelocityX = v[0].is_number();
				a.setVelocityY = v[1].is_number();
				if (a.setVelocityX) a.velocityX = v[0].get<float>();
				if (a.setVelocityY) a.velocityY = v[1].get<float>();
			}
			if (j.contains("impulse") && j["impulse"].is_array() && j["impulse"].size() == 2)
			{
				a.impulse = true;
				a.impulseX = j["impulse"][0].get<float>();
				a.impulseY = j["impulse"][1].get<float>();
			}
			a.damage = j.value("damage", false);
			a.timerMs = j.value("timerMs", 0);
			a.flip = j.value("flip", -1);
			return a;
		}
	};
}
