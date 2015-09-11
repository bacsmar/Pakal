#pragma once
#include "Config.h"
#include "MeshComponent.h"


namespace irr { namespace video { class ITexture; } }
namespace irr { namespace scene { class IMeshSceneNode; } }
namespace irr { namespace scene { class IMesh; } }

namespace Pakal
{
	class IrrGraphicsSystem;
	
	class _PAKALExport MeshComponent_Irrlitch : public MeshComponent
	{
	public:
		DECLARE_RTTI_WITH_BASE(MeshComponent_Irrlitch,MeshComponent)
	protected:
		IrrGraphicsSystem*			m_system;

		irr::video::ITexture*		m_texture;
		irr::scene::IMesh*			m_mesh;
		irr::scene::IMeshSceneNode* m_node;

	public:
		explicit MeshComponent_Irrlitch(IrrGraphicsSystem* irrGraphicsSystem) : m_system(irrGraphicsSystem), m_texture(nullptr), m_mesh(nullptr), m_node(nullptr) {};
		~MeshComponent_Irrlitch();

		BasicTaskPtr  set_mesh(const std::string& meshName) override;
		BasicTaskPtr  set_texture(const std::string& textureName) override;

		void set_position(const tmath::vector3df& position) override;
		tmath::vector3df get_position() override;

		tmath::vector3df get_size() override;
		void set_size(const tmath::vector3df& size) override;

		void set_angle(const tmath::vector3df& angle) override;
		tmath::vector3df get_angle() override;

		BasicTaskPtr initialize(const Settings& settings) override;
		BasicTaskPtr destroy() override;
	};

}
