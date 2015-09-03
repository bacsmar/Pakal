#pragma once
#include "Config.h"
#include "math/tm.h"
#include "TaskFwd.h"
#include "Component.h"

namespace Pakal
{

	enum class BodyType
	{
		Static,
		Kinematic,
		Dynamic
	};

	struct BodyShape
	{
		virtual ~BodyShape() {}

		DECLARE_RTTI(BodyShape)
	};

	struct CircleShape : BodyShape
	{
		DECLARE_RTTI_WITH_BASE(CircleShape,BodyShape)
		float radius;
	};


	class _PAKALExport BodyComponent : public Component
	{
	public:	
		DECLARE_RTTI_WITH_BASE(BodyComponent,Component);
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
		virtual BasicTaskPtr destroy() = 0;

		virtual BasicTaskPtr set_position(const tmath::vector3df& newPosition) = 0;
		virtual tmath::vector3df get_position() = 0;

		virtual void set_density(float density) = 0;
		virtual void set_friction(float friction) = 0;
		virtual void set_restitution(float restitution) = 0;

		virtual float get_density() = 0;
		virtual float get_friction() = 0;
		virtual float get_restitution() = 0;
	};	
}