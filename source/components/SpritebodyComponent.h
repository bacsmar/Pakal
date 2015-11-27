#pragma once
#include "Config.h"
#include "math/tm.h"
#include "TaskFwd.h"
#include "Component.h"
#include "Shapes.h"

namespace Pakal
{
	struct SpritePhysicsLoader;

	class _PAKALExport SpritebodyComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(SpritebodyComponent,Component);
	public:		
		virtual BasicTaskPtr initialize(const SpritePhysicsLoader& loader) = 0;
		virtual BasicTaskPtr terminate() = 0;

		virtual BasicTaskPtr set_position(const tmath::vector3df& newPosition) = 0;
		virtual tmath::vector3df get_position() = 0;

		virtual BasicTaskPtr  set_angle(const tmath::vector3df& position) = 0;
		virtual tmath::vector3df get_angle() = 0;

		virtual tmath::vector3df get_scale() = 0;
		virtual void set_scale(const tmath::vector3df& size) = 0;		

		inline void apply_impulse(float x, float y) { apply_impulse({x,y}); }
		virtual void apply_impulse(const tmath::vector2df& force) = 0;

		inline void apply_force(float x, float y) { apply_force({ x,y }); }
		virtual void apply_force(const tmath::vector2df& force) = 0;
		virtual tmath::vector2df get_lineal_velocity() const = 0;

		inline void set_lineal_velocity(float x, float y) { set_lineal_velocity({x,y}); }
		virtual void set_lineal_velocity(const tmath::vector2df& velocity) = 0;

		virtual void set_fixed_rotation(bool val) = 0;
	};	
}