#pragma once

#include "Config.h"
#include "math/tm.h"
#include "Event.h"

namespace Pakal
{
	class _PAKALExport IWindowManager
	{
	public:
		virtual ~IWindowManager() {}

		Event<void> closed_event;
		Event<tmath::vector2di> resized_event;

		virtual void* create_window(const tmath::vector2di& dimensions, bool fullscreen,unsigned int bitsPerPixel) = 0;
		virtual void  close_window() = 0;
		virtual void  process_os_events() = 0;
	};
}