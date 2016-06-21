#pragma once
 
#include "irrlicht.h"
#include <map>
#include "CBatchingMesh.h"
#include <vector>

namespace irr {namespace scene {
	class CMeshSceneNode;
}
}

//! Grid scene node
namespace Pakal 
{	
	class SceneNodeBatcher : public irr::scene::ISceneNode
	{
		irr::scene::ISceneManager* m_scene_manager;
		irr::scene::CBatchingMesh* m_batching_mesh;
		std::vector<irr::scene::IMeshSceneNode*> m_nodes;
		ISceneNode*	m_batch_root_node;
		bool	m_dirty = true;
		void refresh();

		void render() override;
		const irr::core::aabbox3d<float>& getBoundingBox() const override;
		virtual void OnRegisterSceneNode() override;

	public:
		virtual ~SceneNodeBatcher();
		explicit SceneNodeBatcher(irr::scene::ISceneManager* sceneManager);

		irr::scene::IMeshSceneNode* add_mesh(irr::scene::IMesh* mesh);
		void remove_node(irr::scene::IMeshSceneNode* mesh);
		void set_dirty();
	};

}