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
	public:
		DECLARE_RTTI_WITH_BASE(BodyComponent,PhysicComponent);

		~BodyComponent() override;
		explicit BodyComponent(Box2DPhysicsSystem* sys);

		b2Body*		createBody(b2BodyDef &bodyDef);
		b2Fixture*	addFixture(b2FixtureDef &fixtureDef);

		core::vector3df&			getPosition();
		void						setPosition(core::vector3df & newPosition);

	protected:
		inline Box2DPhysicsSystem* getSystem();
		void on_init() override;
		void on_destroy() override;

		b2Body* m_body;
		b2Fixture* m_fixture;
	private:
		core::vector3df m_Position;
	};	


	class BodyComponent_Box2d : public BodyComponent
	{
	public:
		explicit BodyComponent_Box2d(Box2DPhysicsSystem* sys)
			: BodyComponent(sys) {}
	};



}