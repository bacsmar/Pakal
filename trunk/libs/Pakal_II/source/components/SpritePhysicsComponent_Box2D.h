#pragma once
#include "Config.h"
#include "SpritePhysicsComponent.h"
#include <unordered_map>


class b2Body;
class b2Fixture;
class b2CircleShape;

struct b2BodyDef;
struct b2FixtureDef; 


namespace Pakal
{	
	class Box2DPhysicsSystem;

	class _PAKALExport SpritebodyComponent_Box2D final : public SpritePhysicsComponent
	{
		DECLARE_RTTI_WITH_BASE(SpritebodyComponent_Box2D,SpritePhysicsComponent);
	public:

		explicit SpritebodyComponent_Box2D(Box2DPhysicsSystem* sys): m_system(sys){}
		~SpritebodyComponent_Box2D();

		BasicTaskPtr initialize(const SpriteSheetPhysics& loader) override;
		BasicTaskPtr terminate() override;		

		tmath::vector3df get_position() override;
		BasicTaskPtr set_position(const tmath::vector3df & newPosition) override;

		BasicTaskPtr set_angle(const tmath::vector3df& angle) override;
		tmath::vector3df get_angle() override;

		void set_scale(const tmath::vector3df& scale) override;
		tmath::vector3df get_scale() override;

		virtual void apply_impulse(const tmath::vector2df& force) override;
		virtual void apply_force(const tmath::vector2df& force) override;
		virtual tmath::vector2df get_lineal_velocity() const override;
		virtual void set_lineal_velocity(const tmath::vector2df& velocity) override;

		bool fixed_rotation() const;

		void set_fixed_rotation(bool val) override;

		void set_type(BodyType type) override;
		void set_gravity_scale(float gravityScale) override;

	protected:
		Box2DPhysicsSystem* m_system = nullptr;
	private:
		std::unordered_map<std::string,b2Body*> m_bodies;
		b2Body*	m_active_body = nullptr;
		float m_normalization_factor = 1.f;
		tmath::vector3df m_scale = {1.f,1.f,1.f};
	};	
}
