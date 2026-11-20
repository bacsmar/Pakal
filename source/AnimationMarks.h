///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: AnimationMarks.h
// Purpose: Named moments of an animation clip ("impact" when the fist lands, "step" when a foot touches
//          the floor): which ones the playback passed between two times, also across the loop's end
//          and played backwards. The model (MeshComponent3D_Bgfx::add_clip_mark) reports them as it
//          plays; they are for the presentation (a sound, a spark). A simulation keeps its own timing
//          as data, since it never reads what is drawn.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace Pakal
{
	struct AnimationMark
	{
		double at;          // from the start of the clip, in the clip's own units (seconds or ticks)
		std::string name;
	};

	// The marks the playback went past going from 'from' to 'to' (time played since the clip started, which grows past
	// 'length' while looping and goes down when played backwards), in the order it met them. A mark exactly at 'from'
	// was already reported; one at 'to' is reported now. Without 'loop' the playback stops at the clip's ends.
	inline std::vector<const AnimationMark*> marks_passed(const std::vector<AnimationMark>& marks, double from, double to, double length, bool loop)
	{
		std::vector<const AnimationMark*> passed;
		if (marks.empty() || length <= 0.0 || from == to)
		{
			return passed;
		}
		if (!loop)
		{
			from = std::fmin(std::fmax(from, 0.0), length);
			to = std::fmin(std::fmax(to, 0.0), length);
			if (from == to) return passed;
		}
		const bool forward = to > from;
		// the laps of the clip the playback touched, from the first to the last
		const double firstLap = std::floor((forward ? from : to) / length);
		const double lastLap = std::floor((forward ? to : from) / length);
		const double laps = lastLap - firstLap + 1.0;
		for (double i = 0.0; i < laps; i += 1.0)
		{
			const double lap = forward ? firstLap + i : lastLap - i;
			if (forward)
			{
				for (const AnimationMark& mark : marks)
				{
					const double at = lap * length + mark.at;
					if (at > from && at <= to) passed.push_back(&mark);
				}
			}
			else
			{
				for (auto mark = marks.rbegin(); mark != marks.rend(); ++mark)
				{
					const double at = lap * length + mark->at;
					if (at < from && at >= to) passed.push_back(&*mark);
				}
			}
		}
		return passed;
	}

	// The marks a model passed, reported as they come, or held while it is posed off its thread (a worker of the
	// graphics system) and reported later, on that thread and in the same order (flush)
	class MarkReports
	{
	public:
		using Call = std::function<void(int clip, const std::string& mark)>;

		void defer(bool on) { m_defer = on; }
		bool deferred() const { return m_defer; }
		size_t held() const { return m_held.size(); }

		void report(int clip, const std::vector<std::string>& marks, const Call& call)
		{
			for (const auto& mark : marks)
			{
				if (m_defer) m_held.emplace_back(clip, mark);
				else if (call) call(clip, mark);
			}
		}

		// The held marks, each once (none are kept: a mark held without a call is dropped, as one reported without it)
		void flush(const Call& call)
		{
			if (m_held.empty()) return;
			const auto marks = std::move(m_held);
			m_held.clear();
			if (!call) return;
			for (const auto& [clip, mark] : marks) call(clip, mark);
		}

	private:
		bool m_defer = false;
		std::vector<std::pair<int, std::string>> m_held;
	};
}
