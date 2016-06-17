#pragma once
 
#include "irrlicht.h"
#include <map>
#include "CBatchingMesh.h"

namespace irr {namespace scene {
	class CMeshSceneNode;
}
}

//! Grid scene node
namespace Pakal 
{
	class BatchSceneNode;

	class SceneNodeBatcher
	{
		unsigned m_mesh_count = 0;
		irr::scene::ISceneManager* m_scene_manager;

		irr::scene::CBatchingMesh* m_batching_mesh;
	public:
		virtual ~SceneNodeBatcher();
		explicit SceneNodeBatcher(irr::scene::ISceneManager* sceneManager);
		BatchSceneNode* add_static_mesh(irr::scene::IMesh* mesh, const irr::core::vector3df& position);
	};

}