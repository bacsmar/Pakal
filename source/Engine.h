#pragma once
#include "Config.h"

namespace Poco
{
	class Thread;
}

namespace Pakal
{

	class IPakalApplication;
	class GraphicsSystem;
	class EventSystem;
	class PhysicsSystem;

	class _PAKALExport Engine 
	{
	public:
		void run(IPakalApplication *application );

		static Engine &instance();

		~Engine();
		Engine();

	protected:
		IPakalApplication	*m_Application;
		EventSystem			*m_EventSystem;
		GraphicsSystem		*m_GraphicsSystem;
		PhysicsSystem		*m_PhysicsSystem;

		Poco::Thread		*m_LogicThread;
		Poco::Thread		*m_PhysicsThread;
		//Poco::Thread		*m_GraphicsThread; //main thread

		static bool			ms_Initialized;

		void update();
		void init();
		
	};
}