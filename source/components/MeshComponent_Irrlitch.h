#pragma once
#include "Config.h"
#include "MeshComponent.h"

#if PAKAL_USE_IRRLICHT == 1



namespace irr { namespace video { class ITexture; } }
namespace irr { namespace scene { class IMeshSceneNode; } }
namespace irr { namespace scene { class IMesh; } }

namespace Pakal
{
	class IrrGraphicsSystem;
	
	class _PAKALExport MeshComponent_Irrlitch : public MeshComponent
	{
	protected:
		irr::video::ITexture		*m_texture;
		irr::scene::IMesh			*m_mesh;
		irr::scene::IMeshSceneNode	*m_node;

		inline IrrGraphicsSystem* get_system();

	public:
		DECLARE_RTTI_WITH_BASE(MeshComponent_Irrlitch,MeshComponent)

		explicit MeshComponent_Irrlitch(IrrGraphicsSystem* irrGraphicsSystem);

		void on_initialize() override;
		void on_destroy() override;

		BasicTaskPtr LoadMeshAsync(const std::string& meshName) override;
		BasicTaskPtr LoadTextureAsync(const std::string& textureName) override;

		void setPosition(const tmath::vector3df& position) override;
		tmath::vector3df getPosition() override;
	};

}



#endif
