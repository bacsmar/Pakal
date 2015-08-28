#pragma once
#include "Config.h"
#include "math/tm.h"

#include "SystemComponent.h"
#include "PhysicsSystem.h"

namespace Pakal
{

	class _PAKALExport BodyComponent : public SystemComponent
	{
	public:		
		DECLARE_RTTI_WITH_BASE(BodyComponent,SystemComponent);

		explicit BodyComponent(PhysicsSystem* physicsSystem): SystemComponent(physicsSystem) {}
		~BodyComponent() {};

		virtual void setPosition(const tmath::vector3df& newPosition) = 0;
		virtual tmath::vector3df& getPosition() = 0;
	
	};	
}