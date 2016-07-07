#pragma once
#include "DynamicMeshComponent.h"


namespace irr
{
	namespace scene
	{
		struct SMesh;
		class IMeshSceneNode;
	}

	namespace video
	{
		class ITexture;
	}
}

namespace Pakal
{

	class IrrGraphicsSystem;


	class _PAKALExport DynamicMeshComponent_Irrlicht : public DynamicMeshComponent
	{
		DECLARE_RTTI_WITH_BASE(DynamicMeshComponent_Irrlicht, DynamicMeshComponent)

		IrrGraphicsSystem* m_system;

		DynamicMesh m_meshGenerator;
		
		irr::scene::SMesh* m_mesh;
		irr::scene::IMeshSceneNode* m_node;

		irr::video::ITexture* m_fillTexture;
		irr::video::ITexture* m_edgeTexture;

		std::vector<VertexInfo> m_vertices;
		UVMappingPtr m_mapping;
	public:
		DynamicMeshComponent_Irrlicht(IrrGraphicsSystem* graphicsSystem);
		~DynamicMeshComponent_Irrlicht();


		std::vector<VertexInfo>& get_vertices() override;
		void set_vertices(const std::vector<VertexInfo>&) override;
		const std::vector<tmath::vector3df>& get_generated_vertices() const override;
		enum  class TextureType {Edge, Fill, };
		void set_texture(const std::string& textureName, TextureType type);
		BasicTaskPtr intialize(const Settings& settings) override;
		BasicTaskPtr terminate() override;
		BasicTaskPtr tesellate() override;
	};
}
