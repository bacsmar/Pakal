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

		~Engine();

	protected:
		IPakalApplication	*m_Application;
		EventSystem			*m_EventSystem;
		GraphicsSystem		*m_GraphicsSystem;

		static bool			ms_Initialized;
		
	};
}