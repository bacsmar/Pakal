#include "Engine.h"
#include "LogMgr.h"
#include "GraphicsSystem.h"
#include "PhysicsSystem.h"
#include "GameStateManager.h"
#include "IPakalApplication.h"

#include "ComponentManager.h"
#include "SoundManager.h"
#include "IInputManager.h"
#include "OSManager.h"
#include "Clock.h"
#include <sstream>
#include "persist/Archive.h"
#include <chrono>


//#include <vld.h>
using namespace Pakal;
//////////////////////////////////////////////////////////////////////////
Engine::~Engine()
{
	stop_timer_system();
	os_manager()->terminate();
	SAFE_DEL(m_graphics_system)
	SAFE_DEL(m_physics_system)
	SAFE_DEL(m_component_manager)
	SAFE_DEL(m_game_state_manager)
	SAFE_DEL(m_sound_manager)	
	LOG_INFO("Closing enigne...");
}
//////////////////////////////////////////////////////////////////////////
Engine::Engine(const Settings& settings) :
	System(settings.uses_thread),
	m_application(nullptr),
	m_graphics_system(nullptr),
	m_physics_system(nullptr),
	m_game_state_manager(nullptr),
	m_component_manager(nullptr)
{
	LOG_INFO("Initializing Pakal Engine Version " PAKAL_VERSION_NAME);	

	//un tio diferente
	os_manager()->initialize(settings.os_manager_settings);

	srand(static_cast<unsigned>(time(nullptr)));
	
	m_game_state_manager = new GameStateManager(this);
	m_component_manager	 = new ComponentManager();
	m_sound_manager		 = settings.sound_manager_allocator(this);

	m_graphics_system	= settings.graphic_system_allocator(this,settings.graphic_system_settings);
	m_physics_system	= settings.physics_system_allocator(this,settings.physic_system_settings);

	this->set_target_fps(settings.max_fps);
	m_physics_system->set_target_fps(settings.physic_system_settings.max_fps);
	m_graphics_system->set_target_fps(settings.graphic_system_settings.max_fps);

	if( settings.physic_system_settings.debug_draw)
	{
		m_graphics_system->add_debug_drawer(m_physics_system->get_debug_drawer());
	}

	init_timer_system();

	m_component_manager->register_provider(*m_graphics_system);
	m_component_manager->register_provider(*m_physics_system);
	m_component_manager->register_provider(*m_sound_manager);
	m_component_manager->register_provider(*os_manager()->get_input_manager());

	register_default_components();
	
	add_system(m_physics_system);
	add_system(m_graphics_system);
}
//////////////////////////////////////////////////////////////////////////
void Engine::run(IPakalApplication* application)
{
	ASSERT(get_state() == SystemState::Created ||  get_state() == SystemState::Terminated);
	ASSERT(application);

	LOG_INFO("Running %s", application->get_name());

	m_application = application;
	
	//Initialize managers
	resource_manager()->initialize();
	input_manager()->initialize();
	m_component_manager->initialize();
	m_game_state_manager->initialize();
	m_sound_manager->initialize();

	//initialize systems
	std::vector<BasicTaskPtr> initializationTasks;
	for(auto s : m_systems)
	{
		initializationTasks.push_back(s->initialize());
	}
	TaskUtils::wait_all(initializationTasks);
	
	// Initialize engine
	initialize()->wait();

	//get the systems we are gonna loop into
	std::vector<ISystem*> threadlessSystems;
	threadlessSystems.reserve(m_systems.size());

	if (!is_threaded()) threadlessSystems.push_back(this);

	for (auto s : m_systems)
		if (!s->is_threaded())
		{
			threadlessSystems.push_back(s);
		}

	//do the loop
	Clock clock;
	unsigned long dt = 0;


	while (get_state() != SystemState::Terminated)
	{
		//Update the caption
		m_graphics_system->set_window_caption(get_systems_fps().c_str());

		//update threadless systems
		for (auto s : threadlessSystems)
		{
			if (s->get_state() != SystemState::Terminated)
			{
				s->update(dt);
			}
		}

		//process window events
		if (get_state() != SystemState::Paused)
		{
			os_manager()->process_window_events();
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			os_manager()->process_window_events();
			clock.restart();
		}			

		//update the dt
		dt = clock.restart().asMilliseconds();
	}

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
	input_manager()->terminate();
	resource_manager()->terminate();
}
//////////////////////////////////////////////////////////////////////////
void Engine::on_update(unsigned long dtMilliSeconds)
{
	m_game_state_manager->update(dtMilliSeconds);
}

//////////////////////////////////////////////////////////////////////////
void Engine::on_initialize()
{
	//m_listener_terminate = os_manager()->event_app_finished.add_listener(std::bind(&Engine::terminate,this));
	m_listener_terminate = os_manager()->event_app_finished.add_listener([=]() {this->terminate(); });

	m_application->start(this);	
}
//////////////////////////////////////////////////////////////////////////
void Engine::on_terminate()
{
	os_manager()->event_app_finished.remove_listener(m_listener_terminate);

	m_application->end(this);
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
		{
			resumeTasks.push_back(s->resume());
		}			
	}

	TaskUtils::wait_all(resumeTasks);
}
//////////////////////////////////////////////////////////////////////////

std::wstring Engine::get_systems_fps()
{
	std::wostringstream ss;

	ss << get_system_name() << L"[" << get_fps() << L"] ";
	for (auto s : m_systems)
	{
		ss << s->get_system_name() << L"[" << s->get_fps() << L"] ";
	}

	return ss.str();
}
