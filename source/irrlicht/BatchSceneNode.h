#pragma once
 
#include "irrlicht.h"
#include <irrlicht/source/Irrlicht/CMeshSceneNode.h>

//! Grid scene node
namespace Pakal 
{
	class SceneNodeBatcher;

	class BatchSceneNode : public irr::scene::IMeshSceneNode
	{
	public:		
		//BatchSceneNode(irr::scene::IMesh* mesh, ISceneNode* parent, irr::scene::ISceneManager* mgr, irr::s32 id);

		BatchSceneNode(ISceneNode* parent, irr::scene::ISceneManager* mgr, irr::s32 id)
			: IMeshSceneNode(parent, mgr, id), m_buffer(nullptr), m_batcher(nullptr)
		{
		}

		void setVisible(bool isVisible) override;
		void remove() override;
		void setScale(const irr::core::vector3df& scale) override;
		void setRotation(const irr::core::vector3df& rotation) override;
		void setPosition(const irr::core::vector3df& newpos) override;
		void setParent(irr::scene::ISceneNode* newParent) override;
		void setMesh(irr::scene::IMesh* mesh) override;

		void render() override;
		const irr::core::aabbox3d<float>& getBoundingBox() const override;
		irr::scene::IMesh* getMesh() override;
		irr::scene::IShadowVolumeSceneNode* addShadowVolumeSceneNode(const irr::scene::IMesh* shadowMesh, irr::s32 id, bool zfailmethod, irr::f32 infinity) override;
		void setReadOnlyMaterials(bool readonly) override;
		bool isReadOnlyMaterials() const override;
	private:
		irr::scene::IDynamicMeshBuffer * m_buffer;
		SceneNodeBatcher* m_batcher;
	};

}