#include <box2d/box2d.h>
#include <vector>
#include "components/PhysicComponent.h"
#include "Box2DPhysicsListeners.h"

namespace Pakal
{
	void ContactListener::BeginContact(b2Contact* contact)
	{
		PhysicComponent* pPhysicComponentA = reinterpret_cast<PhysicComponent*>(contact->GetFixtureA()->GetBody()->GetUserData().pointer);
		PhysicComponent* pPhysicComponentB = reinterpret_cast<PhysicComponent*>(contact->GetFixtureB()->GetBody()->GetUserData().pointer);
		if (pPhysicComponentA && pPhysicComponentB)
		{
			pPhysicComponentA->on_collide(*pPhysicComponentB);
			pPhysicComponentB->on_collide(*pPhysicComponentA);
		}
	}

	void ContactListener::EndContact(b2Contact* contact)
	{
		PhysicComponent* pPhysicComponentA = reinterpret_cast<PhysicComponent*>(contact->GetFixtureA()->GetBody()->GetUserData().pointer);
		PhysicComponent* pPhysicComponentB = reinterpret_cast<PhysicComponent*>(contact->GetFixtureB()->GetBody()->GetUserData().pointer);
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