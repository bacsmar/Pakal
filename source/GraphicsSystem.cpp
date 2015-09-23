#include "GraphicsSystem.h"

using namespace Pakal;


//////////////////////////////////////////////////////////////////////////

void GraphicsSystem::on_initialize()
{
	m_os_manager
		->setup_window(0, m_settings.resolution, m_settings.full_screen, m_settings.bits)
		->continue_with(bind(&GraphicsSystem::on_init_graphics,this,std::placeholders::_1),THIS_THREAD)
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