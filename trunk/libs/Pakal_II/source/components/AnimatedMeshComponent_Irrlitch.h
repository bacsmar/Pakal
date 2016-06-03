#pragma once
#include "Config.h"
#include "AnimatedMeshComponent.h"


namespace irr { namespace video { class ITexture; } }
namespace irr { namespace scene {
	class IAnimatedMesh;
	class IAnimatedMeshSceneNode;
	class IMeshSceneNode; } }
namespace irr { namespace scene { class IMesh; } }

namespace Pakal
{
	class IrrGraphicsSystem;
	
	class _PAKALExport AnimatedMeshComponent_Irrlitch : public AnimatedMeshComponent
	{
		DECLARE_RTTI_WITH_BASE(AnimatedMeshComponent_Irrlitch, AnimatedMeshComponent)	
	protected:

		IrrGraphicsSystem*			m_system;
		irr::video::ITexture*		m_texture;
		irr::scene::IAnimatedMesh*	m_mesh;
		irr::scene::IAnimatedMeshSceneNode* m_node;		

	public:
		explicit AnimatedMeshComponent_Irrlitch(IrrGraphicsSystem* irrGraphicsSystem) : m_system(irrGraphicsSystem), m_texture(nullptr), m_mesh(nullptr), m_node(nullptr) {};
		~AnimatedMeshComponent_Irrlitch();

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
