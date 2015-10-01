#pragma once
#include "Config.h"
#include "System.h"

#include <unordered_set>
#include <functional>
#include <algorithm>

#include "PhysicsSystem.h"
#include "GraphicsSystem.h"
#include "OSManager.h"
#include "ResourceManager.h"

namespace Pakal
{
	class IPakalApplication;
	class EventScheduler;
	class GameStateManager;
	class ComponentManager;
	class IManager;
	class SoundManager;
	class IInputManager;

	class _PAKALExport Engine final : public System
	{
		using System::initialize;

	public:

		struct _PAKALExport Settings
		{
			bool uses_thread;
			PhysicsSystem::Settings physic_system_settings;
			GraphicsSystem::Settings graphic_system_settings;

			std::function<GraphicsSystem*(Engine*,const GraphicsSystem::Settings& settings)> graphic_system_allocator;
			std::function<PhysicsSystem*(Engine*,const PhysicsSystem::Settings& settings)>	physics_system_allocator;
			std::function<SoundManager*(Engine*)>	sound_manager_allocator;
			std::function<IInputManager*(Engine*)>	input_manager_allocator;


			Settings();
		};


		explicit Engine(const Settings& settings);
		~Engine();

		void run(IPakalApplication* application);

		inline ComponentManager*	get_component_manager() const { return m_component_manager; }
		inline GraphicsSystem*		get_graphics_system() const { return m_graphics_system; }
		inline PhysicsSystem*		get_physics_system() const { return m_physics_system; }
		inline IPakalApplication*	get_application() const { return m_application; }
		inline SoundManager*		get_sound_manager() const { return m_sound_manager; }
		inline IInputManager*		get_input_manager() const { return m_input_manager; }
		inline GameStateManager*	get_game_state_manager() const { return m_game_state_manager; }
		inline OSManager*			get_os_manager()  { return &OSManager::instance(); } 
		inline ResourceManager*		get_resource_manager()  { return &ResourceManager::instance(); } 


		const char* get_system_name() override { return "Engine"; };

		inline void add_system(ISystem* system)
		{
			ASSERT(get_state() == SystemState::Created || get_state() == SystemState::Terminated);
			ASSERT(std::find(m_systems.begin(),m_systems.end(), system) == m_systems.end());

			m_systems.push_back(system);
		}

		inline void remove_system(ISystem* system)
		{
			ASSERT(get_state() == SystemState::Created || get_state() == SystemState::Terminated);
			m_systems.erase(std::find(m_systems.begin(),m_systems.end(), system));
		}

	private:

		void on_update(long long dt) override;
		void on_initialize() override;
		void on_terminate() override;
		void on_pause() override;
		void on_resume() override;

		IPakalApplication*  m_application;

		GraphicsSystem*		m_graphics_system;
		PhysicsSystem*		m_physics_system;

		GameStateManager*	m_game_state_manager;
		ComponentManager*	m_component_manager;
		SoundManager*		m_sound_manager;
		IInputManager*		m_input_manager;

		std::vector<ISystem*> m_systems;

		unsigned long long m_listener_terminate;
	};
}