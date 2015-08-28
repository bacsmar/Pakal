#pragma once
#include "Config.h"

#include "TaskFwd.h"
#include "SystemComponent.h"

#include "GraphicsSystem.h"
#include "math/tm.h"

namespace Pakal
{

	class _PAKALExport MeshComponent : public SystemComponent
	{
	public:		
		DECLARE_RTTI_WITH_BASE(MeshComponent,SystemComponent);

		explicit MeshComponent(GraphicsSystem* graphicsSystem): SystemComponent(graphicsSystem) {}
		~MeshComponent() {};

		virtual BasicTaskPtr LoadMeshAsync(const std::string& meshName) = 0;
		virtual BasicTaskPtr LoadTextureAsync(const std::string& textureName) = 0;
		virtual void  setPosition(const tmath::vector3df& position) = 0;
		virtual tmath::vector3df getPosition() = 0;
	};	
}