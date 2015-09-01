#pragma once
#include "Config.h"
#include "BodyComponent.h"

class b2Body;
class b2Fixture;
class b2CircleShape;

struct b2BodyDef;
struct b2FixtureDef; 


namespace Pakal
{
	class Box2DPhysicsSystem;

	class _PAKALExport BodyComponent_Box2D : public BodyComponent
	{
	public:
		DECLARE_RTTI_WITH_BASE(BodyComponent_Box2D,BodyComponent);

		explicit BodyComponent_Box2D(Box2DPhysicsSystem* sys);
		~BodyComponent_Box2D();

		b2Body*		createBody(b2BodyDef &bodyDef);
		b2Fixture*	addFixture(b2FixtureDef &fixtureDef);

		tmath::vector3df& getPosition() override;
		void setPosition(const tmath::vector3df & newPosition) override;

	protected:

		inline Box2DPhysicsSystem* get_system();
		void on_initialize() override;
		void on_destroy() override;

		b2Body* m_body;
		b2Fixture* m_fixture;
		tmath::vector3df m_position;
	};	
}
