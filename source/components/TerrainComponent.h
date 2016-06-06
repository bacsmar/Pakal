#pragma once

#include "Component.h"
#include <BasicTask.h>

namespace Pakal
{
	class _PAKALExport TerrainComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(TerrainComponent, Component);
	public:
		virtual ~TerrainComponent(){}

		struct Settings
		{
			std::string terrain_file_name;
			std::string terrain_texture;
			std::string terrain_detail_texture;
		};

		virtual BasicTaskPtr initialize(const Settings& settings) = 0;
		virtual BasicTaskPtr destroy() = 0;

		virtual void show() = 0;
		virtual void hide() = 0;
		virtual void  set_position(const tmath::vector3df& position) = 0;
	};
}
