#include "PhysicsSystem.h"
#include "LogMgr.h"

using namespace Pakal;

#if PAKAL_USE_BOX2D == 1
	#include "box2D/IrrGraphicsSystem.h"
#endif

Pakal::PhysicsSystem* Pakal::PhysicsSystem::createPhysicsSystem()
{
#if PAKAL_USE_BOX2D == 1
	return new Pakal::Box2DPhysicsSystem();
#endif

	return new Pakal::PhysicsSystem();
}

void Pakal::PhysicsSystem::run()
{
	
}
