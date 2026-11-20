#include "Engine.h"
#include "LogMgr.h"
#include "GraphicsSystem.h"
#include "PhysicsSystem.h"
#include "GameStateManager.h"
#include "IPakalApplication.h"
#include "InputManager_Polling.h"

#include "ComponentManager.h"
#include "EntityManager.h"
#include "SoundManager.h"
#include "IInputManager.h"
#include "OSManager.h"
#include "Clock.h"
#include <sstream>
#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include "IUIManager.h"
#include "Profiler.h"


//#include <vld.h>
using namespace Pakal;
//////////////////////////////////////////////////////////////////////////
Engine::~Engine()
{
	if (m_label_flag_thread.joinable()) m_label_flag_thread.join();
	stop_timer_system();
	os_manager()->terminate();
	SAFE_DEL(m_graphics_system)
	SAFE_DEL(m_physics_system)
	SAFE_DEL(m_component_manager)
	SAFE_DEL(m_entity_manager)
	SAFE_DEL(m_game_state_manager)
	SAFE_DEL(m_sound_manager)	
	LOG_INFO("Closing enigne...");
}
//////////////////////////////////////////////////////////////////////////
Engine::Engine(const Settings& settings) :
	System(settings.uses_thread)
{
	// which copy this is, first: a log, a capture or a crash report always says it (C-BLD-1)
	m_build = BuildIdentity::load();
	LOG_INFO("%s", m_build.describe().c_str());
	LOG_INFO("Initializing Pakal Engine Version " PAKAL_VERSION_NAME);

	// a host that cleared an allocator (or one that made nothing) is told so, not left to crash on a null system
	if (!settings.sound_manager_allocator || !settings.graphic_system_allocator || !settings.physics_system_allocator)
	{
		LOG_ERROR("[Engine] Settings without an allocator for the %s", !settings.sound_manager_allocator ? "sound manager"
			: !settings.graphic_system_allocator ? "graphics system" : "physics system");
		throw std::invalid_argument("Pakal::Engine: Settings lacks a system allocator");
	}

	//un tio diferente
	os_manager()->initialize(settings.os_manager_settings);

	srand(static_cast<unsigned>(time(nullptr)));
	
	m_game_state_manager = new GameStateManager(this);
	m_component_manager	 = new ComponentManager();
	m_entity_manager	 = new EntityManager(m_component_manager);
	m_sound_manager		 = settings.sound_manager_allocator(this);

	m_graphics_system	= settings.graphic_system_allocator(this,settings.graphic_system_settings);
	m_physics_system	= settings.physics_system_allocator(this,settings.physic_system_settings);

	if (!m_sound_manager || !m_graphics_system || !m_physics_system)
	{
		LOG_ERROR("[Engine] an allocator made no %s", !m_sound_manager ? "sound manager" : !m_graphics_system ? "graphics system" : "physics system");
		SAFE_DEL(m_graphics_system)
		SAFE_DEL(m_physics_system)
		SAFE_DEL(m_sound_manager)
		SAFE_DEL(m_entity_manager)
		SAFE_DEL(m_component_manager)
		SAFE_DEL(m_game_state_manager)
		os_manager()->terminate();
		throw std::runtime_error("Pakal::Engine: a system allocator made nothing");
	}

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
	m_component_manager->register_provider(*m_graphics_system->get_ui_interface());
	m_component_manager->register_provider(*os_manager()->get_input_manager());

	register_default_components();
	register_default_entities();
	
	add_system(m_physics_system);
	add_system(m_graphics_system);

	// Production: a store bundle asks whether it was promoted to production; its label waits for the answer (at most the request's
	// timeout) and stays when there is none
	if (m_build.channel == "store" && !m_build.label_flag_url.empty())
	{
		m_build_label_state = 1;
		m_label_flag_thread = std::thread([this]()
		{
			std::string answer;
			const int status = os_manager()->http_request("GET", m_build.label_flag_url, {}, {}, &answer, 3000);
			const bool hidden = status == 200 && label_hidden_by_flag(answer, m_build.code);
			LOG_INFO("[Build] label flag: HTTP %d, the label %s", status, hidden ? "hidden (production)" : "shown");
			m_build_label_state = hidden ? 2 : 0;
			show_build_label();
		});
	}
	show_build_label();
}
//////////////////////////////////////////////////////////////////////////
void Engine::show_build_label()
{
	if (IUIManager* ui = m_graphics_system ? m_graphics_system->get_ui_interface() : nullptr)
		ui->set_build_label(build_label_shown() ? m_build.label() : std::string(), m_build_label_corner);
}
//////////////////////////////////////////////////////////////////////////
void Engine::set_build_label_corner(BuildLabelCorner corner)
{
	m_build_label_corner = corner;
	show_build_label();
}
//////////////////////////////////////////////////////////////////////////
bool Engine::build_label_shown() const
{
	return m_build_label_state == 0 && !m_build.label().empty();
}
//////////////////////////////////////////////////////////////////////////
void Engine::run(IPakalApplication* application)
{
	ASSERT(get_state() == SystemState::Created ||  get_state() == SystemState::Terminated);
	ASSERT(application);

	LOG_INFO("Running %s", application->get_name());

	m_application = application;
	m_update_count = 0;
	
	
	//Initialize managers
	resource_manager()->initialize();
	input_manager()->initialize();
	
	// Initialize InputManager_Polling wrapper for convenient key polling
	InputManager_Polling::initialize(input_manager());
	
	m_component_manager->initialize();
	m_entity_manager->initialize();
	m_game_state_manager->initialize();
	m_sound_manager->initialize();

	//m_application->setup_resources(*this);

	//initialize systems
	std::vector<BasicTaskPtr> initializationTasks;
	for(auto s : m_systems)
	{
		initializationTasks.push_back(s->initialize());
	}
	TaskUtils::wait_all(initializationTasks);
	
	// Initialize engine
	auto engineInitTask = initialize();
	if (engineInitTask)
	{
		engineInitTask->wait(false);  // Non-blocking wait to process tasks while waiting
	}

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
	long long carry_us = 0;   // what whole milliseconds leave out, so the time does not run slow

	if (get_state() != SystemState::Terminated)
	{
		LOG_INFO("[Engine] Entering main game loop");
		// Per run, not per process (they were statics: a second run of an engine, or a second engine, kept the first's).
		// The environment is read once the game has started (it may set PAKAL_TITLE in its start).
		int loop_count = 0;
		const char* title = std::getenv("PAKAL_TITLE");		// a game's own window title, instead of the systems' FPS
		const bool logFps = std::getenv("PAKAL_FPS_LOG") != nullptr;	// one log line per second with every system's FPS
		auto lastLog = std::chrono::steady_clock::now();
		
		while (get_state() != SystemState::Terminated)
		{
			if (loop_count++ == 0)
			{
				LOG_INFO("[Engine] First iteration of game loop");
			}
			
			// The caption: the systems' FPS, or a game's own title (set once)
			if (!title)
			{
				m_graphics_system->set_window_caption(get_systems_fps().c_str());
			}
			else if (loop_count == 1)
			{
				m_graphics_system->set_window_caption(title);
			}

			// Performance runs: one log line per second with every system's FPS (PAKAL_FPS_LOG=1)
			if (logFps && std::chrono::steady_clock::now() - lastLog >= std::chrono::seconds(1))
			{
				lastLog = std::chrono::steady_clock::now();
				LOG_INFO("[PERF] %s", get_systems_fps().c_str());
			}
			// A measured run with a length (PAKAL_PROFILE_SECONDS) ends by itself, its summary written
			if (Profiler::get().time_is_up())
			{
				Profiler::get().finish();
				os_manager()->event_app_finished();
			}

			//update threadless systems
			for (auto s : threadlessSystems)
			{
				// Check Engine state before updating each system - allows quick exit on termination
				if (get_state() == SystemState::Terminated)
				{
					LOG_INFO("[Engine] Loop terminating, state=Terminated");
					break;
				}
				
				if (s->get_state() != SystemState::Terminated)
				{
					if (loop_count == 1)
					{
						LOG_INFO("[Engine] Updating system, state=%d", (int)s->get_state());
					}
					{
						// each system of the main loop by its name (a system that waits for the frame shows it here)
						PAKAL_PROFILE_SCOPE(s->get_system_name());
						s->update(dt);
					}
					if (loop_count == 1)
					{
						LOG_INFO("[Engine] System update completed");
					}
				}
			}

			//process window events
			if (get_state() != SystemState::Paused)
			{
				if (loop_count == 1)
				{
					LOG_INFO("[Engine] Processing window events...");
				}
				os_manager()->process_window_events();
				if (loop_count == 1)
				{
					LOG_INFO("[Engine] Window events processed, state=%d", (int)get_state());
				}
			}
			else
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				os_manager()->process_window_events();
				clock.restart();
			}			

			//update the dt
			const long long us = clock.restart().asMicroseconds() + carry_us;
			dt = static_cast<unsigned long>(us / 1000);
			carry_us = us % 1000;
		}
	}

	Profiler::get().finish();
	m_game_state_manager->terminate();


	//terminate systems
	std::vector<BasicTaskPtr> terminationTasks;

	for(auto s : m_systems)
	{
		if (s->get_state() != SystemState::Terminated)
		{
			terminationTasks.push_back(s->terminate());
		}
	}
	
	TaskUtils::wait_all(terminationTasks);

	//terminate managers
	m_sound_manager->terminate();
	m_entity_manager->terminate();
	m_component_manager->terminate();
	input_manager()->terminate();
	resource_manager()->terminate();
}
//////////////////////////////////////////////////////////////////////////
void Engine::on_update(unsigned long dtMilliSeconds)
{
	if (m_update_count++ == 0)
	{
		LOG_INFO("[Engine] on_update() called for first time, dt=%llu ms", dtMilliSeconds);
	}
	
	// files that changed on the disk (dev and --tools builds: docs/UI_RELOAD.md), their callbacks on this, the game's thread
	resource_manager()->dispatch_changes();

	{
		// the game's own work on its thread (its states, their simulation and AI): a slow frame outside the graphics
		// thread's sections shows here
		PAKAL_PROFILE_SCOPE("game.update");
		m_game_state_manager->update(dtMilliSeconds);
		m_entity_manager->process_pending_disposals();
	}
	
	if (m_update_count == 1)
	{
		LOG_INFO("[Engine] on_update() completed, state=%d", (int)get_state());
	}
}

//////////////////////////////////////////////////////////////////////////
void Engine::on_initialize()
{
	m_listener_terminate = os_manager()->event_app_finished.add_listener([this]() {this->terminate(); });
	m_listener_focus = os_manager()->event_window_focused.add_listener([this](const bool& focused)
	{
		m_sound_focused = focused;
		apply_sound_pause();
	});

	m_application->start(*this);	
}
//////////////////////////////////////////////////////////////////////////
void Engine::on_terminate()
{
	
	os_manager()->event_app_finished.remove_listener(m_listener_terminate);
	os_manager()->event_window_focused.remove_listener(m_listener_focus);

	m_application->end(*this);
	
}
//////////////////////////////////////////////////////////////////////////

void Engine::on_pause()
{
	LOG_INFO("%s", get_systems_fps().c_str());
	std::vector<BasicTaskPtr> pauseTaks;

	for (auto s : m_systems)
	{
		if (s->get_state() == SystemState::Running)
			pauseTaks.push_back(s->pause());
	}

	TaskUtils::wait_all(pauseTaks);
	m_sound_engine_paused = true;
	apply_sound_pause();
}
//////////////////////////////////////////////////////////////////////////

void Engine::on_resume()
{
	m_sound_engine_paused = false;
	apply_sound_pause();
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

void Engine::apply_sound_pause()
{
	const bool paused = !m_sound_focused || m_sound_engine_paused;
	LOG_INFO("[Engine] sound %s (focus %d, engine paused %d)", paused ? "held" : "plays", m_sound_focused ? 1 : 0, m_sound_engine_paused ? 1 : 0);
	if (m_sound_manager) m_sound_manager->set_paused(paused);
}
//////////////////////////////////////////////////////////////////////////

std::string Engine::get_systems_fps()
{
	std::ostringstream ss;

	ss << get_system_name() << "[" << get_fps() << "] ";
	for (auto s : m_systems)
	{
		ss << s->get_system_name() << "[" << s->get_fps() << "] ";
	}

	return ss.str();
}
