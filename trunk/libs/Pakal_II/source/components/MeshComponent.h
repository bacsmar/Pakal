#pragma once
#include "Config.h"

#include "TaskFwd.h"
#include "Component.h"

#include "GraphicsSystem.h"
#include "math/tm.h"

namespace Pakal
{

	class _PAKALExport MeshComponent : public Component
	{
	public:		
		DECLARE_RTTI_WITH_BASE(MeshComponent,Component);

		explicit MeshComponent(GraphicsSystem* graphicsSystem): Component(graphicsSystem) {}
		~MeshComponent() {};

		virtual BasicTaskPtr LoadMeshAsync(const std::string& meshName) = 0;
		virtual BasicTaskPtr LoadTextureAsync(const std::string& textureName) = 0;
		virtual void  setPosition(const tmath::vector3df& position) = 0;
		virtual tmath::vector3df getPosition() = 0;
	};	
}