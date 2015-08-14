#pragma once
#include "Config.h"
#include "System.h"

namespace Pakal
{
	class AsyncTaskDispatcher;

	class IPakalApplication;
	class GraphicsSystem;
	class EventScheduler;
	class PhysicsSystem;
	class GameStateManager;
	class ComponentManager;

	class _PAKALExport Engine : public System
	{
	public:
		static Engine &instance();

		void run(IPakalApplication* application);


		inline ComponentManager*	get_component_manager() const { return m_component_manager; }
		inline GraphicsSystem*		get_graphics_system() const { return m_graphics_system; }
		inline PhysicsSystem*		get_physics_system() const { return m_physics_system; }
		inline EventScheduler*		get_scheduler() const { return m_event_scheduler; }
		inline IPakalApplication*	get_application() const { return m_application; }

		Engine();
		~Engine();
	protected:

		void procress_os_messages();
		void initialize()  override;
		void on_update() override;
		void on_initialize() override;
		void on_terminate() override;

		IPakalApplication*  m_application;
		EventScheduler*		m_event_scheduler;

		GraphicsSystem*		m_graphics_system;
		PhysicsSystem*		m_physics_system;

		GameStateManager*	m_game_state_manager;
		ComponentManager*	m_component_manager;

		AsyncTaskDispatcher* m_dispatcher; 

	};
}