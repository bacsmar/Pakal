#include "GraphicsSystem.h"

using namespace Pakal;


//////////////////////////////////////////////////////////////////////////

void GraphicsSystem::on_initialize()
{
	m_os_manager
		->setup_window(0, m_settings.resolution, m_settings.full_screen, m_settings.bits)
		->continue_with(std::bind(&GraphicsSystem::on_init_graphics,this,std::placeholders::_1),THIS_THREAD)
		->wait();
}
//////////////////////////////////////////////////////////////////////////

void GraphicsSystem::on_terminate()
{
	on_terminate_graphics();
}

//////////////////////////////////////////////////////////////////////////
void GraphicsSystem::on_update(long long dt)
{
	{	// updatables lock
		std::lock_guard<std::mutex> lock(m_updatablesMutex);
		for (auto & updatable : m_updatables)
		{
			updatable->update(dt);
		}
	}
	on_update_graphics(dt);
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
	std::lock_guard<std::mutex> lock(m_updatablesMutex);
	m_updatables.emplace_back(updatable);
	
}

void GraphicsSystem::remove_from_update_list(IUpdatable* updatable)
{
	std::lock_guard<std::mutex> lock(m_updatablesMutex);
	const auto &it = std::find(m_updatables.begin(), m_updatables.end(), updatable);
	
	ASSERT(it != m_updatables.end());
	
	m_updatables.erase(it);
	
}