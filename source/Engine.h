#pragma once
#include "Config.h"
#include "System.h"

#include <unordered_set>

namespace Pakal
{
	class IPakalApplication;
	class GraphicsSystem;
	class EventScheduler;
	class PhysicsSystem;
	class GameStateManager;
	class ComponentManager;
	class IManager;

	class _PAKALExport Engine : public System
	{
	private:
		using System::initialize;

		bool m_running_loop;

	public:
		static Engine &instance();

		void run(IPakalApplication* application);

		inline ComponentManager*	get_component_manager() const { return m_component_manager; }
		inline GraphicsSystem*		get_graphics_system() const { return m_graphics_system; }
		inline PhysicsSystem*		get_physics_system() const { return m_physics_system; }
		inline IPakalApplication*	get_application() const { return m_application; }

		const char* get_system_name() override { return "Engine"; };

		inline void add_system(ISystem* system)
		{
			ASSERT_IF(get_state() != SystemState::Created && get_state() != SystemState::Terminated);
			ASSERT_IF(std::find(m_systems.begin(),m_systems.end(), system) != m_systems.end());

			m_systems.push_back(system);
		}


		Engine();
		~Engine();
	protected:

		void procress_os_messages();
		void on_update() override;
		void on_initialize() override;
		void on_terminate() override;
		void on_pause() override;
		void on_resume() override;

		IPakalApplication*  m_application;

		GraphicsSystem*		m_graphics_system;
		PhysicsSystem*		m_physics_system;

		GameStateManager*	m_game_state_manager;
		ComponentManager*	m_component_manager;
		EventScheduler*		m_scheduler;

		std::vector<ISystem*> m_systems;

	};
}