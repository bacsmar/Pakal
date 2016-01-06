#include "SimpleTimer.h"

namespace Pakal
{
	void SimpleTimer::set_interval(unsigned ms)
	{
		m_milliseconds = ms;
		m_clock.restart();
	}

	bool SimpleTimer::expired() const
	{
		return m_clock.getElapsedTime().asMilliseconds() > m_milliseconds;
	}
}