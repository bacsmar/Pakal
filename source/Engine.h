#pragma once
#include "Config.h"

namespace Poco
{
	class Thread;
}
//TODO shutdown of engine
namespace Pakal
{
	class IPakalApplication;
	class GraphicsSystem;
	class EventSystem;
	class PhysicsSystem;
	class GameStateSystem;

	class _PAKALExport Engine 
	{
	public:
		void start(IPakalApplication *application );

		static Engine &instance();

		~Engine();
		Engine();

	protected:
		IPakalApplication	*m_Application;
		EventSystem			*m_EventSystem;
		GraphicsSystem		*m_GraphicsSystem;
		PhysicsSystem		*m_PhysicsSystem;
		GameStateSystem		*m_GameStateSystem;
		
		Poco::Thread		*m_LogicThread;

		static bool			ms_Initialized;

		void update();
		void init();
		
	};
}