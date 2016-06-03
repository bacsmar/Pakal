#pragma once

#include "MeshComponent.h"

namespace Pakal
{

	//class IAnimation{};

	class _PAKALExport AnimatedMeshComponent : public MeshComponent //, public IAnimation
	{
		DECLARE_RTTI_WITH_BASE(AnimatedMeshComponent, MeshComponent);
	public:
		virtual ~AnimatedMeshComponent(){}
	};	
}