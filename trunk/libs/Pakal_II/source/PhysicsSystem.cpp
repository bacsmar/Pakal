#include "PhysicsSystem.h"
#include "persist/Archive.h"


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
}

void PhysicsSystem::on_pause()
{
	pause_world();
}

void PhysicsSystem::on_resume()
{
	resume_world();
}

void PhysicsSystem::Settings::persist(Archive* archive)
{
	archive->value("uses_thread", uses_thread);
	archive->value("allow_sleep", allow_sleep);
	archive->value("velocity_iterations", velocity_iterations);
	archive->value("position_iterations", position_iterations);
	archive->value("debug_draw", debug_draw);
	archive->value("max_fps", max_fps);
	archive->value("gravity_x", gravity.x);
	archive->value("gravity_y", gravity.y);
	archive->value("gravity_z", gravity.z);
}