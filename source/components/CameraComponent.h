#pragma once
#include "Component.h"
#include "BasicTask.h"
#include "PakalMath.h"

namespace Pakal
{
	class _PAKALExport CameraComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(CameraComponent, Component);
	public:

		struct Settings
		{
			tmath::vector3df position;
			tmath::vector3df look_at;
		};

		virtual BasicTaskPtr initialize(const Settings& settings) = 0;
		virtual BasicTaskPtr terminate() = 0;

		virtual void set_rotation(const tmath::vector3df & rotation) = 0;
		virtual tmath::vector3df get_rotation() const = 0;

		virtual BasicTaskPtr set_target(const tmath::vector3df & targetPosition)= 0;
		virtual BasicTaskPtr set_position(const tmath::vector3df & position) = 0;
		virtual tmath::vector3df get_position() const = 0;

		// TODO: better implementation
		virtual void set_target_and_position(const tmath::vector3df & target, const tmath::vector3df & position) = 0;
		virtual tmath::vector3df translate_cordinate_to_point(const Pakal::tmath::vector2df & cordinate, const Pakal::tmath::vector2df & screen_size) = 0;
		virtual tmath::vector3df cast_ray_from_screen_point(const Pakal::tmath::vector2df & cordinate, const Pakal::tmath::vector2df & screen_size) = 0;		

		virtual void bind_target_and_rotation(bool bound) = 0;
	
		virtual ~CameraComponent(){}

	};
}