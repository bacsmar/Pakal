#pragma once

#include <atomic>
#include <cmath>

namespace Pakal
{
class FPSCounter
{
public:
	FPSCounter()
	: m_fps(60), m_elapsed_milliseconds(0), m_frames_counted(0)
	{
	}

	unsigned get_fps() const
	{
		return m_fps.load(std::memory_order_relaxed);
	}

	void register_frame(unsigned long dt)
	{
		++m_frames_counted;
		m_elapsed_milliseconds += dt;		

		if (m_elapsed_milliseconds >= 500 )
		{
			m_fps.store(static_cast<unsigned>(ceil ( ( 1000.0 * m_frames_counted ) / m_elapsed_milliseconds)), std::memory_order_relaxed);

			m_frames_counted = 0;			
			m_elapsed_milliseconds = 0;
		}
	}

private:

	// written by the system's thread, read by any (the engine shows every system's rate)
	std::atomic<unsigned> m_fps;
	unsigned long m_elapsed_milliseconds;
	unsigned m_frames_counted;
};


} // pakal



