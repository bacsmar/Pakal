#pragma once
#include "config.h"
#include <Pakalvector3d.h>
#include "components/GraphicComponent.h"

#include "TaskFwd.h"

namespace irr { namespace video { class ITexture; } }
namespace irr { namespace scene { class IMeshSceneNode; } }
namespace irr { namespace scene { class IMesh; } }

namespace Pakal
{
	class IrrGraphicsSystem;


	class _PAKALExport MeshComponent : public GraphicComponent
	{
	public:		

		DECLARE_RTTI(MeshComponent);

		~MeshComponent() override;

		MeshComponent(IrrGraphicsSystem* irr);
	protected:
		inline IrrGraphicsSystem* getSystem();

		irr::video::ITexture		*m_texture;
		irr::scene::IMesh			*m_mesh;
		irr::scene::IMeshSceneNode	*m_node;
		void onInit() override;
		void onDestroy() override;
	public:

		BasicTaskPtr LoadMeshAsync(const std::string& meshName);
		BasicTaskPtr LoadTextureAsync(const std::string& textureName);
		inline void  setPosition(const Pakal::core::vector3df& position);
		inline  Pakal::core::vector3df getPosition();
	};	
}