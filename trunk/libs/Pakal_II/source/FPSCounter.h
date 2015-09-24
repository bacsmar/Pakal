#pragma once

namespace Pakal
{
class FPSCounter
{
public:
	FPSCounter::FPSCounter()
	: m_fps(60), m_elapsed_milliseconds(0), m_frames_counted(0)
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
		m_elapsed_milliseconds += dt;		

		if (m_elapsed_milliseconds >= 1500 )
		{
			m_fps = static_cast<unsigned>(ceil ( ( 1000.0 * m_frames_counted ) / m_elapsed_milliseconds));

			m_frames_counted = 0;			
			m_elapsed_milliseconds = 0;
		}
	}

private:

	unsigned m_fps;	
	long long m_elapsed_milliseconds;
	unsigned m_frames_counted;
};


} // pakal



