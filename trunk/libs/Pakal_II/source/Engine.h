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
	class EventScheduler;
	class PhysicsSystem;
	class GameStateSystem;
	class ComponentSystem;
	class EntitySystem;

	class _PAKALExport Engine 
	{
	public:
		void start(IPakalApplication *application );

		static Engine &instance();

		~Engine();
		Engine();

		inline EntitySystem		* getEntitySystem() const { return m_EntitySystem; }
		inline ComponentSystem	* getComponentSystem() const { return m_ComponentSystem; }
		inline GraphicsSystem	* getGraphicsSystem() const { return m_GraphicsSystem; }

	protected:
		IPakalApplication	*m_Application;
		EventScheduler		*m_EventScheduler;

		GraphicsSystem		*m_GraphicsSystem;
		PhysicsSystem		*m_PhysicsSystem;

		GameStateSystem		*m_GameStateSystem;
		ComponentSystem		*m_ComponentSystem;
		EntitySystem		*m_EntitySystem;

		Poco::Thread		*m_LogicThread;

		static bool			ms_Initialized;

		bool				m_shouldTerminate;

		void run();
		void init();
		
	};
}