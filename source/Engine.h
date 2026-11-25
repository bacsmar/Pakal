#pragma once
#include <atomic>
#include "BuildIdentity.h"
#include <thread>
#include "Config.h"
#include "System.h"

#include <unordered_set>
#include <functional>

#include "PhysicsSystem.h"
#include "GraphicsSystem.h"
#include "OSManager.h"
#include "ResourceManager.h"

namespace Pakal
{
	class IPakalApplication;
	class EventScheduler;
	class GameStateManager;
	class EntityManager;
	class ComponentManager;
	class IManager;
	class SoundManager;
	class IInputManager;
	class IUIManager;
	class Archive;

	class _PAKALExport Engine final : public System
	{
		using System::initialize;

	public:

		struct _PAKALExport Settings
		{
			bool uses_thread = true; // Engine runs in its own thread
			unsigned max_fps = 1000;  // High FPS for game logic in separate thread
			PhysicsSystem::Settings physic_system_settings;
			GraphicsSystem::Settings graphic_system_settings;
			OSManager::Settings os_manager_settings;

			std::function<GraphicsSystem*(Engine*,const GraphicsSystem::Settings& settings)> graphic_system_allocator;
			std::function<PhysicsSystem*(Engine*,const PhysicsSystem::Settings& settings)>	physics_system_allocator;
			std::function<SoundManager*(Engine*)>	sound_manager_allocator;

			void persist(Archive* archive);
			Settings();
		};		

		explicit Engine(const Settings& settings);
		~Engine();

		void run(IPakalApplication* application);

		inline ComponentManager*	component_manager() const { return m_component_manager; }
		inline EntityManager*		entity_manager() const { return m_entity_manager; }
		inline GraphicsSystem*		graphics_system() const { return m_graphics_system; }
		inline PhysicsSystem*		physics_system() const { return m_physics_system; }
		inline IPakalApplication*	get_application() const { return m_application; }
		inline SoundManager*		sound_manager() const { return m_sound_manager; }
		inline IInputManager*		input_manager() const { return os_manager()->get_input_manager(); }
		inline GameStateManager*	game_state_manager() const { return m_game_state_manager; }
		inline OSManager*			os_manager() const  { return &OSManager::instance(); } 
		inline ResourceManager*		resource_manager() const { return &ResourceManager::instance(); }
		inline IUIManager*			get_ui_manager() const { return m_graphics_system->get_ui_interface(); }		

		// Which copy this is (Assets/build.json, or build.json beside a desktop build): logged as the run's first line
		inline const BuildIdentity& build_identity() const { return m_build; }
		// The build's label, drawn by the engine outside production (C-ID-7): in the corner of the game's version text
		// (bottom left unless the game moves it). A promoted production bundle hides it (BuildIdentity::label_flag_url)
		void set_build_label_corner(BuildLabelCorner corner);
		bool build_label_shown() const;

		const char* get_system_name() override { return nameof(Engine); };

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

		void on_update(unsigned long dtMilliSeconds) override;
		void on_initialize() override;
		void on_terminate() override;
		void on_pause() override;
		void on_resume() override;
		void register_default_components() const;	void register_default_entities() const;		std::string get_systems_fps();
		static void init_timer_system();	// timer.cpp
		static void stop_timer_system();	// timer.cpp

		void show_build_label();
		BuildIdentity		m_build;
		BuildLabelCorner	m_build_label_corner = BuildLabelCorner::BottomLeft;
		std::atomic<int>	m_build_label_state{ 0 };   // 0: shown, 1: waiting for the label flag, 2: hidden by it
		std::thread			m_label_flag_thread;

		IPakalApplication*  m_application = nullptr;

		GraphicsSystem*		m_graphics_system = nullptr;
		PhysicsSystem*		m_physics_system = nullptr;

		GameStateManager*	m_game_state_manager = nullptr;
		ComponentManager*	m_component_manager = nullptr;
		EntityManager*		m_entity_manager = nullptr;
		SoundManager*		m_sound_manager = nullptr;

		std::vector<ISystem*> m_systems;

		EventId m_listener_terminate = 0;
		EventId m_listener_focus = 0;   // the sound is held while no window of the app has the focus
		// The sound plays only while the app has the focus AND the engine is not paused. Each of the two used to set the pause
		// alone, the last one winning: a phone that gave the focus back behind its lock screen (locked with its button)
		// played the music with the engine still paused
		std::atomic<bool> m_sound_focused{ true }, m_sound_engine_paused{ false };   // (the focus comes from the platform's thread)
		void apply_sound_pause();
		unsigned m_update_count = 0;	// the engine's updates in this run (its first one is logged)
	};
}