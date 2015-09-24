#pragma once

namespace Pakal
{
class FPSCounter
{
public:
	FPSCounter::FPSCounter()
	: m_fps(60), m_start_time(0), m_frames_counted(0)
	{
	}
	//! returns current fps
	unsigned get_fps() const
	{
		return m_fps;
	}

	//! to be called every frame
	void register_frame(long long dt)
	{
		++m_frames_counted;
		m_start_time += dt;
		const auto milliseconds = m_start_time;

		if (milliseconds >= 1500 )
		{
			const float invMilli = 1.f /( (float) milliseconds );

			m_fps = ceil ( ( 1000 * m_frames_counted ) * invMilli );

			m_frames_counted = 0;			
			m_start_time = 0;
		}
	}

private:

	unsigned m_fps;	
	long long m_start_time;
	unsigned m_frames_counted;
};


} // pakal



