#pragma once
#include "Config.h"
#include "PakalMath.h"
#include "TaskFwd.h"
#include "PhysicComponent.h"
#include "SpritePhysics.h"

namespace Pakal
{
	class _PAKALExport SpritePhysicsComponent : public PhysicComponent
	{
		DECLARE_RTTI_WITH_BASE(SpritePhysicsComponent, PhysicComponent);
	public:	

		enum BodyType
		{
			StaticBody = 0,
			//KinematicBody,
			DynamicBody,
		};

		virtual BasicTaskPtr initialize(const SpriteSheetPhysics& loader) = 0;
		virtual BasicTaskPtr terminate() = 0;

		virtual BasicTaskPtr set_position(const tmath::vector3df& newPosition) = 0;
		virtual tmath::vector3df get_position() = 0;

		virtual BasicTaskPtr  set_angle(const tmath::vector3df& position) = 0;
		virtual tmath::vector3df get_angle() = 0;

		virtual float get_scale() = 0;
		virtual void set_scale(float scale) = 0;

		inline void apply_impulse(float x, float y) { apply_impulse({x,y}); }
		virtual void apply_impulse(const tmath::vector2df& force) = 0;

		inline void apply_force(float x, float y) { apply_force({ x,y }); }
		virtual void apply_force(const tmath::vector2df& force) = 0;
		virtual tmath::vector2df get_lineal_velocity() const = 0;

		inline void set_lineal_velocity(float x, float y) { set_lineal_velocity({x,y}); }
		virtual void set_lineal_velocity(const tmath::vector2df& velocity) = 0;

		virtual void set_fixed_rotation(bool val) = 0;

		virtual void set_type(BodyType type) = 0;
		virtual void set_gravity_scale(float gravityScale) = 0;
	};	
}
