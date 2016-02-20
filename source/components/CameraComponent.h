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

		virtual void set_target(const tmath::vector3df& targetPosition)= 0;

		virtual void set_position(tmath::vector3df position) = 0;
		virtual tmath::vector3df  get_position() const = 0;
	
		virtual ~CameraComponent(){}

	};
}