#include "SceneNodeBatcher.h"
#include <LogMgr.h>
#include <irrlicht/source/Irrlicht/CMeshSceneNode.h>
#include <vector>
#include "BatchSceneNode.h"

using namespace irr;

Pakal::SceneNodeBatcher::~SceneNodeBatcher()
{
	m_batching_mesh->drop();
}

Pakal::SceneNodeBatcher::SceneNodeBatcher(irr::scene::ISceneManager* sceneManager): m_scene_manager(sceneManager) 
{
	m_batching_mesh = new irr::scene::CBatchingMesh();
	//m_scene_manager->addMeshSceneNode(m_batching_mesh);
	//m_batching_mesh->drop();
}

Pakal::BatchSceneNode* Pakal::SceneNodeBatcher::add_static_mesh(scene::IMesh* mesh, const irr::core::vector3df& position)
{	
	m_batching_mesh->addMesh(mesh, position);
	m_mesh_count++;
	m_batching_mesh->getSourceBufferCount();
	if (m_mesh_count == 2000)
	{
		m_batching_mesh->update();
		//m_batching_mesh->setHardwareMappingHint(scene::EHM_STATIC, scene::EBT_VERTEX_AND_INDEX);
		m_batching_mesh->setHardwareMappingHint(scene::EHM_STREAM, scene::EBT_VERTEX_AND_INDEX);
		//m_batching_mesh->setHardwareMappingHint(scene::EHM_DYNAMIC, scene::EBT_VERTEX_AND_INDEX);
		//m_batching_mesh->setHardwareMappingHint(scene::EHM_NEVER, scene::EBT_VERTEX_AND_INDEX);
		auto *node = m_scene_manager->addMeshSceneNode(m_batching_mesh);
		//node->setScale(core::vector3df(10, 10, 10));
		//node->setDebugDataVisible(scene::EDS_BBOX_BUFFERS);
	}
	return new Pakal::BatchSceneNode(nullptr, m_scene_manager, -1);;
}