#pragma once
#include "Config.h"

#include "TaskFwd.h"
#include "Component.h"

#include "math/tm.h"

namespace Pakal
{

	class _PAKALExport MeshComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(MeshComponent, Component);
	public:				

		struct Settings
		{
			std::string mesh_name;
			std::string texture_name;
			tmath::vector3df position;
			tmath::vector3df size;
		};

		virtual BasicTaskPtr initialize(const Settings& settings) = 0;
		virtual BasicTaskPtr destroy() = 0;

		virtual BasicTaskPtr set_mesh(const std::string& meshName) = 0;
		virtual BasicTaskPtr set_texture(const std::string& textureName) = 0;

		virtual void  set_position(const tmath::vector3df& position) = 0;
		virtual tmath::vector3df get_position() = 0;

		virtual tmath::vector3df get_size() = 0;
		virtual void set_size(const tmath::vector3df& size) = 0;

		virtual void  set_angle(const tmath::vector3df& position) = 0;
		virtual tmath::vector3df get_angle() = 0;
	//protected:
		virtual ~MeshComponent(){}
	};	
}