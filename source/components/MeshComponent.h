#pragma once
#include "Config.h"

#include "TaskFwd.h"
#include "Component.h"

#include "math/tm.h"

namespace Pakal
{

	class _PAKALExport MeshComponent : public Component
	{
	public:		
		DECLARE_RTTI_WITH_BASE(MeshComponent,Component);

		struct Settings
		{
			std::string MeshName;
			std::string TextureName;
			tmath::vector3df Position;
		};

		virtual BasicTaskPtr initialize(const Settings& settings) = 0;
		virtual BasicTaskPtr destroy() = 0;

		virtual BasicTaskPtr set_mesh(const std::string& meshName) = 0;
		virtual BasicTaskPtr set_texture(const std::string& textureName) = 0;

		virtual void  set_position(const tmath::vector3df& position) = 0;
		virtual tmath::vector3df get_position() = 0;
	};	
}