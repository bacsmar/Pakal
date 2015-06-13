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

	protected:
		IPakalApplication	*m_Application;
		EventSystem			*m_EventSystem;
		GraphicsSystem		*m_GraphicsSystem;
		PhysicsSystem		*m_PhysicsSystem;
		GameStateSystem		*m_GameStateSystem;

		ComponentSystem		*m_ComponentSystem;		
		EntitySystem		*m_EntitySystem;		

		Poco::Thread		*m_LogicThread;

		static bool			ms_Initialized;

		void update();
		void init();
		
	};
}