#include <Box2D.h>
#include <vector>
#include "Components/PhysicComponent.h"
#include "Box2DPhysicsListeners.h"

namespace Pakal
{
	void ContactListener::BeginContact(b2Contact* contact)
	{
		PhysicComponent* pPhysicComponentA = (PhysicComponent*)contact->GetFixtureA()->GetBody()->GetUserData();
		PhysicComponent* pPhysicComponentB = (PhysicComponent*)contact->GetFixtureB()->GetBody()->GetUserData();
		if (pPhysicComponentA && pPhysicComponentB)
		{
			pPhysicComponentA->on_collide(*pPhysicComponentB);
			pPhysicComponentB->on_collide(*pPhysicComponentA);
		}
	}

	void ContactListener::EndContact(b2Contact* contact)
	{
		PhysicComponent* pPhysicComponentA = (PhysicComponent*)contact->GetFixtureA()->GetBody()->GetUserData();
		PhysicComponent* pPhysicComponentB = (PhysicComponent*)contact->GetFixtureB()->GetBody()->GetUserData();
		if (pPhysicComponentA && pPhysicComponentB)
		{
			pPhysicComponentA->on_end_collide(*pPhysicComponentB);
			pPhysicComponentB->on_end_collide(*pPhysicComponentA);
		}
	}

	void ContactListener::PreSolve(b2Contact* contact, const b2Manifold* oldManifold)
	{
	}
}