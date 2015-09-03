#include "PhysicsSystem.h"


using namespace Pakal;


PhysicsSystem::PhysicsSystem(const Settings& settings):  System(settings.uses_thread), m_settings(settings)
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

void PhysicsSystem::on_update(long long dt)
{
	update_world(dt);
	updated.notify();
}

void PhysicsSystem::on_pause()
{
	pause_world();
}

void PhysicsSystem::on_resume()
{
	resume_world();
}