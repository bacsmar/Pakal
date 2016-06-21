#include "SceneNodeBatcher.h"
#include <LogMgr.h>
#include <irrlicht/source/Irrlicht/CMeshSceneNode.h>
#include <vector>
#include <algorithm>

using namespace irr;

void Pakal::SceneNodeBatcher::refresh()
{	
	m_batching_mesh->clear();
	for (auto node : m_nodes)
	{
		m_batching_mesh->addMesh(node->getMesh(), node->getPosition(), node->getRotation(),
			node->getScale());
	}
	LOG_DEBUG("[SceneNode batcher] refreshing %d meshes", m_batching_mesh->getSourceBufferCount());	
	m_batching_mesh->update();
	m_batching_mesh->setHardwareMappingHint(scene::EHM_STREAM, scene::EBT_VERTEX_AND_INDEX);
	if(m_batch_root_node)
		m_batch_root_node->remove();
	m_batch_root_node = m_scene_manager->addMeshSceneNode(m_batching_mesh);	
	m_batch_root_node->setVisible(m_nodes.size() > 0);
	m_dirty = false;
	LOG_INFO("[SceneNode batcher] done");
}

void Pakal::SceneNodeBatcher::render()
{
	if (m_dirty)
		refresh();
	auto count = m_batching_mesh->getSourceBufferCount();
}

const irr::core::aabbox3d<float>& Pakal::SceneNodeBatcher::getBoundingBox() const
{
	return m_batching_mesh->getBoundingBox();
}

void Pakal::SceneNodeBatcher::OnRegisterSceneNode()
{
	if (IsVisible)
	{
		SceneManager->registerNodeForRendering(this);
	}

	ISceneNode::OnRegisterSceneNode();
}

Pakal::SceneNodeBatcher::~SceneNodeBatcher()
{
	m_batching_mesh->drop();
}

Pakal::SceneNodeBatcher::SceneNodeBatcher(irr::scene::ISceneManager* sceneManager) :
	ISceneNode(sceneManager->getRootSceneNode(), sceneManager, -1),
	m_scene_manager(sceneManager), m_batch_root_node(nullptr)
{
	m_batching_mesh = new irr::scene::CBatchingMesh();
	m_scene_manager->addMeshSceneNode(m_batching_mesh);
	//m_batch_root_node = m_scene_manager->addMeshSceneNode(m_batching_mesh);
}

scene::IMeshSceneNode* Pakal::SceneNodeBatcher::add_mesh(scene::IMesh* mesh)
{
	auto node = m_scene_manager->addMeshSceneNode(mesh);
	node->grab();
	node->setParent(nullptr);
	m_nodes.push_back(node);
	set_dirty();

	return node;
}

void Pakal::SceneNodeBatcher::remove_node(irr::scene::IMeshSceneNode* mesh)
{
	std::remove_if(m_nodes.begin(), m_nodes.end(), 
		[=](const irr::scene::IMeshSceneNode* s) { return s == mesh; });
	set_dirty();
}

void Pakal::SceneNodeBatcher::set_dirty()
{
	m_dirty = true;
	m_batching_mesh->clear();
}
