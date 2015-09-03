#include "Engine.h"
#include "LogMgr.h"
#include "GraphicsSystem.h"
#include "PhysicsSystem.h"
#include "GameStateManager.h"
#include "IPakalApplication.h"
#include "EventScheduler.h"

#include "ComponentManager.h"
#include "SoundManager.h"
#include "ResourceManager.h"

#ifdef PAKAL_WIN32_PLATFORM
	#include <Windows.h>
#endif
 
using namespace Pakal;

//////////////////////////////////////////////////////////////////////////
Engine::~Engine()
{
	SAFE_DEL(m_graphics_system)
	SAFE_DEL(m_physics_system)
	SAFE_DEL(m_component_manager)
	SAFE_DEL(m_game_state_manager)
	SAFE_DEL(m_sound_manager)
	SAFE_DEL(m_resource_manager)
}
//////////////////////////////////////////////////////////////////////////
Engine::Engine(const Settings& settings) :
	System(settings.use_threads),
	m_running_loop(false),
	m_application(nullptr),
	m_graphics_system(nullptr),
	m_physics_system(nullptr),
	m_game_state_manager(nullptr),
	m_component_manager(nullptr)
{
	LOG_INFO("Initializing Pakal Engine Version " PAKAL_VERSION_NAME);

	LogMgr::instance();	

	m_resource_manager	 = new ResourceManager();
	m_game_state_manager = new GameStateManager(this);
	m_component_manager	 = new ComponentManager();
	m_sound_manager		 = settings.sound_manager_allocator(this);

	m_graphics_system	= settings.graphic_system_allocator(this);
	m_physics_system	= settings.physics_system_allocator(this);

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

	LOG_INFO("Running unnamed application ");

	m_application = application;

	//exit in case the graphics_system exits
	m_running_loop = true;
	auto listenerId = m_graphics_system->terminate_requested.add_listener([this]()
	{
		m_running_loop = false;
	});

	//Initialize managers
	m_component_manager->initialize();
	m_game_state_manager->initialize();
	m_sound_manager->initialize();
	m_resource_manager->initialize();

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

	std::chrono::system_clock::time_point start,end;
	long long delta = 0;

	//do the loop
	while(m_running_loop)
	{
		start = std::chrono::high_resolution_clock::now();

		for (auto s : nonThreadedSystems)
		{
			if (s->get_state() != SystemState::Terminated)
				s->update(delta);
		}
		procress_os_messages();

		end = std::chrono::high_resolution_clock::now();
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

	//terminate managers
	m_sound_manager->terminate();
	m_component_manager->terminate();
	m_game_state_manager->terminate();
	m_resource_manager->terminate();

	//unsubscribe from event
	m_graphics_system->terminate_requested.remove_listener(listenerId);

	//clear the message queue
	procress_os_messages();
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