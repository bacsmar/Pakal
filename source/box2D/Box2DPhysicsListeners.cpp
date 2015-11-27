#pragma once

#include <Box2D.h>
#include <vector>
#include "Components/PhysicComponent.h"
#include "Box2DPhysicsListeners.h"

namespace Pakal
{	
	void ContactListener::PreSolve(b2Contact* contact, const b2Manifold* oldManifold)
	{
		PhysicComponent* pPhysicComponentA = (PhysicComponent*)contact->GetFixtureA()->GetBody()->GetUserData();
		PhysicComponent* pPhysicComponentB = (PhysicComponent*)contact->GetFixtureB()->GetBody()->GetUserData();
		if (pPhysicComponentA && pPhysicComponentB)
		{
			pPhysicComponentA->on_collide(*pPhysicComponentB);
		}
	}
}