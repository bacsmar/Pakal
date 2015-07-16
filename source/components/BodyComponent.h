#pragma once
#include "config.h"
#include "components/PhysicComponent.h"
#include "TaskFwd.h"
#include "Pakalvector3d.h"

class b2Body;
class b2Fixture;
class b2CircleShape;

struct b2BodyDef;
struct b2FixtureDef; 

namespace Pakal
{
	class Box2DPhysicsSystem;

	class _PAKALExport BodyComponent : public PhysicComponent
	{
		DECLARE_RTTI(BodyComponent);
	public:
		void onInit() override;
		void onDestroy() override;
		~BodyComponent() override;

		BodyComponent(Box2DPhysicsSystem* sys);

		b2Body	*	createBody(b2BodyDef &bodyDef);
		b2Fixture*	addFixture(b2FixtureDef &fixtureDef);

		Pakal::core::vector3df &	getPosition();
		void						setPosition(Pakal::core::vector3df & newPosition);

	protected:
		inline Box2DPhysicsSystem* getSystem();

		b2Body *m_body;
		b2Fixture *m_fixture;
	private:
		Pakal::core::vector3df m_Position;
		unsigned int listenerId ;
	};	
}