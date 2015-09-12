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

	class _PAKALExport BodyComponent_Box2D final : public BodyComponent
	{
	public:
		DECLARE_RTTI_WITH_BASE(BodyComponent_Box2D,BodyComponent);

		explicit BodyComponent_Box2D(Box2DPhysicsSystem* sys): m_system(sys), m_body(nullptr), m_fixture(nullptr) {}
		~BodyComponent_Box2D();

		BasicTaskPtr initialize(const Settings& settings) override;
		BasicTaskPtr destroy() override;

		void set_density(float density) override;
		void set_friction(float friction) override;
		void set_restitution(float restitution) override;
		float get_density() override;
		float get_friction() override;
		float get_restitution() override;

		tmath::vector3df get_position() override;
		BasicTaskPtr set_position(const tmath::vector3df & newPosition) override;

		BasicTaskPtr set_angle(const tmath::vector3df& angle) override;
		tmath::vector3df get_angle() override;

		void set_size(const tmath::vector3df& size) override;
		tmath::vector3df get_size() override;

	protected:
		Box2DPhysicsSystem* m_system;

	private:
		b2Body* m_body;
		b2Fixture* m_fixture;
	};	
}
