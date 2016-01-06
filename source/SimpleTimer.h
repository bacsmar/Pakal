#pragma once

#include "Config.h"
#include "Clock.h"

namespace Pakal
{
	// Simple timer, it doesn't notify when expires.
	class _PAKALExport SimpleTimer
	{
		Clock m_clock;
		unsigned m_milliseconds = 0;
	public:
		void set_interval(unsigned ms);
		bool expired() const;
	};

}