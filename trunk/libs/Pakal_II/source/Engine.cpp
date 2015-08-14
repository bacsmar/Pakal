#include "Engine.h"
#include "LogMgr.h"
#include "GraphicsSystem.h"
#include "PhysicsSystem.h"
#include "GameStateManager.h"
#include "IPakalApplication.h"
#include "EventScheduler.h"

#include "ComponentManager.h"
#include "SingletonHolder.h"

#include "Poco/Foundation.h"

using namespace Pakal;

//////////////////////////////////////////////////////////////////////////
Engine& Engine::instance()
{
	static SingletonHolder<Engine> sh;
	return *sh.get();
}
//////////////////////////////////////////////////////////////////////////
Engine::~Engine()
{
	SAFE_DEL(m_graphics_system);
	SAFE_DEL(m_physics_system)
	SAFE_DEL(m_component_manager)
	SAFE_DEL(m_game_state_manager)
	SAFE_DEL(m_event_scheduler);

	SAFE_DEL(m_application);
	SAFE_DEL(m_dispatcher);
	LogMgr::stop();
}
//////////////////////////////////////////////////////////////////////////
Engine::Engine() :
	System(PAKAL_USE_THREADS == 1),
	m_application(nullptr),
	m_event_scheduler(nullptr),
	m_graphics_system(nullptr),
	m_physics_system(nullptr),
	m_game_state_manager(nullptr),
	m_component_manager(nullptr),
	m_dispatcher(nullptr)
{
	LogMgr::init();
	LogMgr::setLogLevel(10);

	m_graphics_system	= GraphicsSystem::create_instance();
	m_physics_system	= PhysicsSystem::createInstance();
	m_game_state_manager	= new GameStateManager(this);
	m_component_manager	= new ComponentManager();
	m_event_scheduler	= new EventScheduler();
	m_dispatcher		= new AsyncTaskDispatcher();


	m_component_manager->registerProvider(*m_graphics_system);
	m_component_manager->registerProvider(*m_physics_system);	

	m_event_scheduler->registerDispatcher(m_graphics_system);
	m_event_scheduler->registerDispatcher(m_physics_system);
	m_event_scheduler->registerDispatcher(m_dispatcher);
}
//////////////////////////////////////////////////////////////////////////
void Engine::run(IPakalApplication* application)
{
	m_application = application;
	initialize();

}
//////////////////////////////////////////////////////////////////////////
void Engine::initialize()
{
	ASSERT(!m_running);

	LOG_INFO("Initializing Pakal Engine Version " PAKAL_VERSION_NAME);
	ASSERT(m_application != nullptr);


	m_component_manager->initialize();
	m_game_state_manager->initialize();
	m_physics_system->initialize();
	m_graphics_system->initialize();
	m_graphics_system->dispatchTasks();
	m_physics_system->dispatchTasks();

	m_application->setup_environment(this);

	System::initialize();

	do
	{
#if PAKAL_USE_THREADS == 0
		on_update();
		m_physics_system->on_update();
#endif
		m_graphics_system->on_update();
		procress_os_messages();
	} 
	while (m_running);

}
//////////////////////////////////////////////////////////////////////////
void Engine::on_initialize()
{
	m_dispatcher->dispatchTasks();
	m_application->setup_game_states(m_game_state_manager);	
}
//////////////////////////////////////////////////////////////////////////
void Engine::on_terminate()
{
	m_component_manager->terminate();
	m_game_state_manager->terminate();
	m_physics_system->terminate();
	m_graphics_system->terminate();
}
//////////////////////////////////////////////////////////////////////////
void Engine::on_update()
{
	m_dispatcher->dispatchTasks();
}
//////////////////////////////////////////////////////////////////////////
void Engine::procress_os_messages()
{
#ifdef PAKAL_WIN32_PLATFORM
	MSG msg;
	while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		printf("Message received %u\n" ,  msg.message);
		if (msg.message == WM_QUIT)
		{
			terminate();
		}
	}

	
#endif
}
//////////////////////////////////////////////////////////////////////////
