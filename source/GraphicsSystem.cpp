#include "GraphicsSystem.h"

#include "IUIManager.h"
#include "persist/Archive.h"

using namespace Pakal;


//////////////////////////////////////////////////////////////////////////

void GraphicsSystem::on_initialize()
{
	OSMgr.setup_window(m_settings.window_id, m_settings.resolution, m_settings.full_screen, m_settings.bits)
		->continue_with(std::bind(&GraphicsSystem::on_init_graphics,this,std::placeholders::_1),THIS_THREAD)
		->wait();	
	m_ui_manager->initialize();
}
//////////////////////////////////////////////////////////////////////////

void GraphicsSystem::on_terminate()
{
	m_ui_manager->terminate();
	on_terminate_graphics();
	OSMgr.close_window();
}

//////////////////////////////////////////////////////////////////////////
void GraphicsSystem::on_update(unsigned long dtMilliSeconds)
{
	{	// updatables lock
		mutex_guard lock(m_updatablesMutex);
		for (auto & updatable : m_updatables)
		{
			updatable->update(static_cast<unsigned>(dtMilliSeconds));
		}
	}
	on_update_graphics(dtMilliSeconds);	
}
//////////////////////////////////////////////////////////////////////////

void GraphicsSystem::on_pause()
{
	on_pause_graphics();
}
//////////////////////////////////////////////////////////////////////////
void GraphicsSystem::on_resume()
{
	on_resume_graphics();
}

void GraphicsSystem::add_to_update_list(IUpdatable* updatable)
{
	mutex_guard lock(m_updatablesMutex);
	m_updatables.emplace_back(updatable);
}

void GraphicsSystem::remove_from_update_list(IUpdatable* updatable)
{
	mutex_guard lock(m_updatablesMutex);
	const auto &it = std::find(m_updatables.begin(), m_updatables.end(), updatable);
	
	ASSERT(it != m_updatables.end());
	
	m_updatables.erase(it);
}

GraphicsSystem::GraphicsSystem(const Settings& settings): System(false), m_settings(settings)
{
	m_ui_manager = settings.ui_manager_allocator(this);
}

GraphicsSystem::~GraphicsSystem()
{
	SAFE_DEL(m_ui_manager);
}

void GraphicsSystem::Settings::persist(Archive* archive)
{
	archive->value("bpp", bits);
	archive->value("full_screen", full_screen);
	archive->value("vsync", vsync);
	archive->value("max_fps", max_fps);
	archive->value("resolution_x", resolution.x);
	archive->value("resolution_y", resolution.y);	
}

void GraphicsSystem::set_window_caption(const char* title)
{
	OSMgr.get_window_manager()->set_title(title);
}
