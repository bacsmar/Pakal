#include "PhysicsSystem.h"

#if PAKAL_USE_BOX2D
	#include "Box2D/Box2DPhysicsSystem.h"
#endif

using namespace Pakal;

PhysicsSystem* PhysicsSystem::createInstance()
{
#if PAKAL_USE_BOX2D
	return new Box2DPhysicsSystem();
#else
	return nullptr;
#endif
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
	dispatchTasks();
	update_event.notify(true);
}