#include "Engine.h"
#include "LogMgr.h"
#include "GraphicsSystem.h"
#include "PhysicsSystem.h"
#include "GameStateManager.h"
#include "IPakalApplication.h"
#include "EventScheduler.h"
#include "cpplinq.hpp"

#include "ComponentManager.h"
#include "SingletonHolder.h"

#ifdef PAKAL_WIN32_PLATFORM
	#include <Windows.h>
#endif

using namespace Pakal;
using namespace cpplinq;

//////////////////////////////////////////////////////////////////////////
Engine& Engine::instance()
{
	static SingletonHolder<Engine> sh;
	return *sh.get();
}
//////////////////////////////////////////////////////////////////////////
Engine::~Engine()
{
	SAFE_DEL(m_graphics_system)
	SAFE_DEL(m_physics_system)
	SAFE_DEL(m_component_manager)
	SAFE_DEL(m_game_state_manager)
	SAFE_DEL(m_scheduler);

	LogMgr::stop();
}
//////////////////////////////////////////////////////////////////////////
Engine::Engine() :
	System(new EventScheduler(),PAKAL_USE_THREADS == 1),
	m_application(nullptr),
	m_graphics_system(nullptr),
	m_physics_system(nullptr),
	m_game_state_manager(nullptr),
	m_component_manager(nullptr),
	m_running_loop(false)
{
	LogMgr::init();
	LogMgr::set_log_level(10);

	m_scheduler = get_scheduler();

	m_graphics_system	= GraphicsSystem::create_instance(m_scheduler);
	m_physics_system	= PhysicsSystem::create_instance(m_scheduler);

	m_game_state_manager	= new GameStateManager(this);
	m_component_manager		= new ComponentManager();

	m_component_manager->register_provider(*m_graphics_system);
	m_component_manager->register_provider(*m_physics_system);

	add_system(m_graphics_system);
	add_system(m_physics_system);

}

//////////////////////////////////////////////////////////////////////////
void Engine::run(IPakalApplication* application)
{
	ASSERT_IF(get_state() != SystemState::Created &&  get_state() != SystemState::Terminated);
	ASSERT_IF(application == nullptr);

	LOG_INFO("Initializing Pakal Engine Version " PAKAL_VERSION_NAME);

	ISystem* engine[] = {this};
	m_running_loop = true;

	//exit in case the graphics_system exits
	auto listenerId = m_graphics_system->terminate_event.add_listener([this]() { m_running_loop = false;  });

	m_application = application;

	//Initialize managers
	m_scheduler->initialize();
	m_component_manager->initialize();
	m_game_state_manager->initialize();

	//initialize systems
	auto initializationTasks = 
			from(m_systems) 
		>>  select([](ISystem* sys){ return sys->initialize();  }) 
		>>  to_vector(m_systems.size());

	TaskUtils::wait_all(initializationTasks);

	// Initialize engine
	initialize();
	
	//get the systems we are gonna loop into
	auto nonThreadedSystems = 
			from(m_systems) 
		>>	concat(from_array(engine))  
		>>	where([](ISystem* s){ return !s->is_threaded(); }) 
		>>	to_vector();

	//do the loop
	while(m_running_loop)
	{
		for (auto s : nonThreadedSystems)
		{
			if (s->get_state() != SystemState::Terminated)
				s->update();
		}
		procress_os_messages();
	}

	//unsubscribe from event
	m_graphics_system->terminate_event.remove_listener(listenerId);
	
	//terminate systems
	auto terminationTasks = 
			from(m_systems)
		>>	concat(from_array(engine))  
		>>	where([](ISystem* s){ return  s->get_state() != SystemState::Terminated;  })
		>>  select([](ISystem* sys){ return sys->terminate();  }) 
		>>  to_vector(m_systems.size());

	TaskUtils::wait_all(terminationTasks);


	//terminate managers
	m_component_manager->terminate();
	m_game_state_manager->terminate();
	m_scheduler->terminate();
}
//////////////////////////////////////////////////////////////////////////
void Engine::on_initialize()
{
	m_application->setup_environment(this);
	m_application->setup_game_states(m_game_state_manager);	
}
//////////////////////////////////////////////////////////////////////////
void Engine::on_terminate()
{
	m_running_loop = false;  
}
//////////////////////////////////////////////////////////////////////////

void Engine::on_pause()
{
	auto tasks = from(m_systems) >> select([](ISystem* s) { return s->pause(); }) >> to_vector(m_systems.size());

	TaskUtils::wait_all(tasks);
}
//////////////////////////////////////////////////////////////////////////

void Engine::on_resume()
{
	auto tasks = from(m_systems) >> select([](ISystem* s) { return s->resume(); }) >> to_vector(m_systems.size());

	TaskUtils::wait_all(tasks);	
}
//////////////////////////////////////////////////////////////////////////

void Engine::on_update() {}

//////////////////////////////////////////////////////////////////////////
void Engine::procress_os_messages()
{
#ifdef PAKAL_WIN32_PLATFORM
	MSG msg;
	while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.message == WM_QUIT)
		{
			m_running_loop = false;
		}
	}
#endif
}
//////////////////////////////////////////////////////////////////////////
