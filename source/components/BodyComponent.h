#pragma once
#include "Config.h"
#include "math/tm.h"

#include "Component.h"
#include "PhysicsSystem.h"

namespace Pakal
{

	class _PAKALExport BodyComponent : public Component
	{
	public:		
		DECLARE_RTTI_WITH_BASE(BodyComponent,Component);

		explicit BodyComponent(PhysicsSystem* physicsSystem): Component(physicsSystem) {}
		~BodyComponent() {};

		virtual void setPosition(const tmath::vector3df& newPosition) = 0;
		virtual tmath::vector3df& getPosition() = 0;
	
	};	
}