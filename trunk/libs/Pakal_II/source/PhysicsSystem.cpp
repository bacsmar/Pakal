#include "PhysicsSystem.h"

#if PAKAL_USE_BOX2D
	#include "Box2D/Box2DPhysicsSystem.h"
#endif

using namespace Pakal;

PhysicsSystem* PhysicsSystem::create_instance(EventScheduler* scheduler)
{
#if PAKAL_USE_BOX2D
	return new Box2DPhysicsSystem(scheduler);
#else
	return nullptr;
#endif
}

PhysicsSystem::PhysicsSystem(EventScheduler* scheduler): System(scheduler, PAKAL_USE_THREADS == 1)
{
	update_event.connect_with_scheduler(scheduler);
}

void PhysicsSystem::on_initialize()
{
	init_world();
}

void PhysicsSystem::on_terminate()
{
	clear_world();
}

void PhysicsSystem::on_update()
{
	update_world();
	update_event.notify(true);
}

void PhysicsSystem::on_pause()
{
	pause_world();
}

void PhysicsSystem::on_resume()
{
	resume_world();
}