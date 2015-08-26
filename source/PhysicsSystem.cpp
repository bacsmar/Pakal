#include "PhysicsSystem.h"


using namespace Pakal;


PhysicsSystem::PhysicsSystem(EventScheduler* scheduler, bool usesThread): 
	System(scheduler,usesThread),
	update_event(scheduler)
{
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