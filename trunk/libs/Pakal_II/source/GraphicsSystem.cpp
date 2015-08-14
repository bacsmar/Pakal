#include "GraphicsSystem.h"

#include "EventScheduler.h"

#if PAKAL_USE_IRRLICHT == 1
#include "irrlicht/IrrGraphicsSystem.h"
#endif

using namespace Pakal;

GraphicsSystem* GraphicsSystem::create_instance()
{
#if PAKAL_USE_IRRLICHT == 1
	return new IrrGraphicsSystem();
#else
	return new NullGraphicSystem();
#endif
}

//////////////////////////////////////////////////////////////////////////
void GraphicsSystem::on_update()
{
	dispatchTasks();
}