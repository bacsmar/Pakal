#pragma once
#include "Config.h"
#include "PakalMath.h"
#include "TaskFwd.h"
#include "Component.h"
#include "Shapes.h"

namespace Pakal
{

	enum class BodyType
	{
		Static,
		Kinematic,
		Dynamic
	};
	


	class _PAKALExport BodyComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(BodyComponent,Component);
	public:	
	struct Settings
	{
		BodyType type;
		BodyShape* shape_info;
		float density;
		float friction;
		float restitution;
		tmath::vector3df position;

		Settings() : type(BodyType::Dynamic), shape_info(nullptr), density(2.37f), friction(0.31f), restitution(0.82f) { }
	};

		virtual BasicTaskPtr initialize(const Settings& settings) = 0;
		virtual BasicTaskPtr terminate() = 0;

		virtual BasicTaskPtr set_position(const tmath::vector3df& newPosition) = 0;
		virtual tmath::vector3df get_position() = 0;

		virtual BasicTaskPtr  set_angle(const tmath::vector3df& position) = 0;
		virtual tmath::vector3df get_angle() = 0;

		virtual tmath::vector3df get_size() = 0;
		virtual void set_size(const tmath::vector3df& size) = 0;

		virtual void set_density(float density) = 0;
		virtual void set_friction(float friction) = 0;
		virtual void set_restitution(float restitution) = 0;

		virtual float get_density() = 0;
		virtual float get_friction() = 0;
		virtual float get_restitution() = 0;
	};	
}