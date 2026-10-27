///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SimulationClock.h
// Purpose: The fixed-step clock of a game's simulation. Frames bring real time; the simulation runs in
//          whole steps of the same length, so it plays the same whatever the frame rate (and two runs
//          with the same seed and input are equal). The frame is drawn 'alpha' of the way from the
//          last step to the next. A long frame (loading, a stall) is cut to 'maxFrame', so the game
//          does not run many steps at once to catch up; while paused the time is let go.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace Pakal
{
	class SimulationClock
	{
	public:
		explicit SimulationClock(float step = 1.0f / 60.0f, float maxFrame = 0.25f) : m_step(step), m_maxFrame(maxFrame) {}

		// Real seconds since the last call (0 the first time), cut to maxFrame. It reads a steady clock: the engine's dt
		// comes in whole milliseconds, which would make the steps uneven.
		float frame_seconds()
		{
			const auto now = std::chrono::steady_clock::now();
			const float seconds = m_started ? std::min(std::chrono::duration<float>(now - m_last).count(), m_maxFrame) : 0.0f;
			m_last = now;
			m_started = true;
			return seconds;
		}

		// Adds a frame's seconds and returns how many steps to run now
		int advance(float seconds)
		{
			m_pending += std::clamp(seconds, 0.0f, m_maxFrame);
			int steps = 0;
			while (m_pending >= m_step)
			{
				m_pending -= m_step;
				++steps;
			}
			m_steps += static_cast<uint64_t>(steps);
			return steps;
		}

		// Paused: the time not played yet is dropped (it is not caught up later)
		void hold() { m_pending = 0.0f; }

		// How far the frame is from the last step to the next (0..1)
		float alpha() const { return std::min(1.0f, m_pending / m_step); }
		float step() const { return m_step; }
		// Steps run so far
		uint64_t steps() const { return m_steps; }

	private:
		float m_step;
		float m_maxFrame;
		float m_pending = 0.0f;
		uint64_t m_steps = 0;
		bool m_started = false;
		std::chrono::steady_clock::time_point m_last;
	};
}
