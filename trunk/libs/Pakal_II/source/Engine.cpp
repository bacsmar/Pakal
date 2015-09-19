#include "Engine.h"
#include "LogMgr.h"
#include "GraphicsSystem.h"
#include "PhysicsSystem.h"
#include "GameStateManager.h"
#include "IPakalApplication.h"
#include "EventScheduler.h"

#include "ComponentManager.h"
#include "SoundManager.h"
#include "IInputManager.h"
#include "OSManager.h"

//#include <vld.h>

using namespace Pakal;
//////////////////////////////////////////////////////////////////////////
Engine::~Engine()
{
	SAFE_DEL(m_graphics_system)
	SAFE_DEL(m_input_manager)
	SAFE_DEL(m_physics_system)
	SAFE_DEL(m_component_manager)
	SAFE_DEL(m_game_state_manager)
	SAFE_DEL(m_sound_manager)	
}
//////////////////////////////////////////////////////////////////////////
Engine::Engine(const Settings& settings) :
	System(settings.uses_thread),
	m_running_loop(false),
	m_application(nullptr),
	m_graphics_system(nullptr),
	m_physics_system(nullptr),
	m_game_state_manager(nullptr),
	m_component_manager(nullptr),
	m_input_manager(nullptr)
{
	LOG_INFO("Initializing Pakal Engine Version " PAKAL_VERSION_NAME);

	LogMgr::instance();	
	
	m_game_state_manager = new GameStateManager(this);
	m_component_manager	 = new ComponentManager();
	m_sound_manager		 = settings.sound_manager_allocator(this);
	m_input_manager		 = settings.input_manager_allocator(this);

	m_graphics_system	= settings.graphic_system_allocator(this,settings.graphic_system_settings);
	m_physics_system	= settings.physics_system_allocator(this,settings.physic_system_settings);

	m_component_manager->register_provider(*m_graphics_system);
	m_component_manager->register_provider(*m_physics_system);

	add_system(m_graphics_system);
	add_system(m_physics_system);
}
//////////////////////////////////////////////////////////////////////////
void Engine::run(IPakalApplication* application)
{
	ASSERT(get_state() == SystemState::Created ||  get_state() == SystemState::Terminated);
	ASSERT(application);

	LOG_WARNING("Running unnamed application ");

	m_application = application;
	m_running_loop = true;

	//Initialize managers
	m_component_manager->initialize();
	m_game_state_manager->initialize();
	m_sound_manager->initialize();
	m_input_manager->initialize();

	//listen for os termination event
	auto listenderId = get_os_manager()->event_window_destroyed.add_listener([this](OSManager::WindowArgs){ m_running_loop = false; });

	//initialize systems
	std::vector<BasicTaskPtr> initializationTasks;
	for(auto s : m_systems)
	{
		initializationTasks.push_back(s->initialize());
	}
	TaskUtils::wait_all(initializationTasks);


	// Initialize engine
	initialize();
	
	//get the systems we are gonna loop into
	std::vector<ISystem*> nonThreadedSystems;
	nonThreadedSystems.reserve(m_systems.size() + 1);

	if (!is_threaded())
		nonThreadedSystems.push_back(this);

	for (auto s : m_systems)
	{
		if (!s->is_threaded())
			nonThreadedSystems.push_back(s);
	}

	long long delta = 0;

	//do the loop
	while(m_running_loop)
	{
		auto start = std::chrono::system_clock::now();

		for (auto s : nonThreadedSystems)
		{
			if (s->get_state() != SystemState::Terminated)
				s->update(delta);
		}

		get_os_manager()->process_os_events();

		auto end = std::chrono::system_clock::now();
		delta = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
	}
	
	//terminate engine
	if (get_state() != SystemState::Terminated)
		terminate()->wait();

	//terminate systems
	std::vector<BasicTaskPtr> terminationTasks;

	for(auto s : m_systems)
	{
		if (s->get_state() != SystemState::Terminated)
			terminationTasks.push_back(s->terminate());
	}
	TaskUtils::wait_all(terminationTasks);

	get_os_manager()->event_window_destroyed.remove_listener(listenderId);

	//terminate managers
	m_sound_manager->terminate();
	m_component_manager->terminate();
	m_game_state_manager->terminate();
	m_input_manager->terminate();
}
//////////////////////////////////////////////////////////////////////////
OSManager* Engine::get_os_manager() const
{
	return &OSManager::instance();
}

//////////////////////////////////////////////////////////////////////////
void Engine::on_initialize()
{
	m_application->setup_environment(this);
	m_application->start(m_game_state_manager);	
}
//////////////////////////////////////////////////////////////////////////
void Engine::on_terminate()
{
	m_application->end(m_game_state_manager);
	m_running_loop = false;  
}
//////////////////////////////////////////////////////////////////////////

void Engine::on_pause()
{
	std::vector<BasicTaskPtr> pauseTaks;

	for (auto s : m_systems)
	{
		if (s->get_state() == SystemState::Running)
			pauseTaks.push_back(s->pause());
	}

	TaskUtils::wait_all(pauseTaks);
}
//////////////////////////////////////////////////////////////////////////

void Engine::on_resume()
{
	std::vector<BasicTaskPtr> resumeTasks;

	for (auto s : m_systems)
	{
		if (s->get_state() == SystemState::Paused)
			resumeTasks.push_back(s->resume());
	}

	TaskUtils::wait_all(resumeTasks);
}
//////////////////////////////////////////////////////////////////////////