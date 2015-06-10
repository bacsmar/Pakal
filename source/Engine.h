#pragma once
#include "Config.h"

namespace Pakal
{

	class IPakalApplication;
	class GraphicsSystem;
	class EventSystem;

	class _PAKALExport Engine 
	{
	public:
		void run(IPakalApplication *application );

		static Engine &instance();
	protected:
		IPakalApplication	*mApplication;
		EventSystem			*mEventSystem;
		GraphicsSystem		*mGraphicsSystem;

		static bool			msInitialized;
	};
}