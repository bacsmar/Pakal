#include "GraphicsSystem.h"

#if PAKAL_USE_IRRLICHT == 1
#include "irrlicht/IrrGraphicsSystem.h"
#endif

using namespace Pakal;


GraphicsSystem* GraphicsSystem::create_instance(EventScheduler* scheduler)
{
#if PAKAL_USE_IRRLICHT == 1
	return new IrrGraphicsSystem(scheduler);
#else
	return new NullGraphicSystem();
#endif
}
//////////////////////////////////////////////////////////////////////////

void GraphicsSystem::on_initialize()
{
	on_init_graphics();
}
//////////////////////////////////////////////////////////////////////////

void GraphicsSystem::on_terminate()
{
	on_terminate_graphics();
	terminate_event.notify();
}

//////////////////////////////////////////////////////////////////////////
void GraphicsSystem::on_update()
{
	on_update_graphics();
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