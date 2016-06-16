#pragma once

#include "Component.h"
#include <BasicTask.h>
#include <PakalMath.h>
#include "persist/Archive.h"

namespace Pakal
{
	class _PAKALExport TerrainComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(TerrainComponent, Component);
	public:
		virtual ~TerrainComponent(){}

		struct Settings
		{
			std::string highmap_file_name;
			std::string terrain_texture;
			std::string terrain_detail_texture;
			void persist(Archive* archive);
		};

		virtual BasicTaskPtr initialize(const Settings& settings) = 0;
		virtual BasicTaskPtr destroy() = 0;

		virtual void show() = 0;
		virtual void hide() = 0;
		virtual void  set_position(const tmath::vector3df& position) = 0;
	};

	inline void TerrainComponent::Settings::persist(Archive* archive)
	{
		archive->value("highmap", highmap_file_name);
		archive->value("texture", terrain_texture);
		archive->value("detail_texture", terrain_detail_texture);
	}
}
