////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

#include "SpriteBatcher.hpp"
#include "Components/Sprite.h"
#include "MaterialManager.h"
#include "CBatchingMesh.h"
#include "SpriteNode_Irrlicht.hpp"

using namespace irr;
using namespace scene;
using namespace Pakal;

SpriteBatcher::SpriteBatcher(irr::scene::ISceneManager* mgr, irr::s32 id) : 
	ISceneNode(mgr->getRootSceneNode(), mgr, id),
	m_scene_manager(mgr)
{
	m_sprites.reserve(256);
	m_batching_mesh = new irr::scene::CBatchingMesh();
	//m_node = m_scene_manager->addMeshSceneNode(m_batching_mesh,this);
	m_node = m_scene_manager->addMeshSceneNode(m_batching_mesh);
}

SpriteBatcher::~SpriteBatcher()
{
	//ISceneNode::removeChild(m_node);
	m_batching_mesh->clear();
	SAFE_DEL(m_batching_mesh);
}

void SpriteBatcher::add_sprite(SpriteNode_Irrlicht* sprite)
{
	m_sprites.push_back(sprite);
	set_dirty();
}

void SpriteBatcher::remove_sprite(SpriteNode_Irrlicht* sprite)
{
	auto found = std::remove(m_sprites.begin(), m_sprites.end(), sprite);
	if( found != m_sprites.end())
		m_sprites.erase(found);

	set_dirty();
}

void SpriteBatcher::set_dirty()
{
	m_is_dirty = true;
	m_batching_mesh->clear();
}

void SpriteBatcher::render()
{
	if (m_is_dirty)
		reset_mesh_buffer();
	else
		refresh_mesh_buffer();
}

const irr::core::aabbox3d<float>& SpriteBatcher::getBoundingBox() const
{
	return m_batching_mesh->getBoundingBox();
}

void SpriteBatcher::OnRegisterSceneNode()
{
	if (IsVisible)
	{
		SceneManager->registerNodeForRendering(this);
	}

	ISceneNode::OnRegisterSceneNode();
}

void SpriteBatcher::reset_mesh_buffer()
{
	m_batching_mesh->clear();
	for (auto sprite : m_sprites)
	{		
		m_batching_mesh->addMeshBuffer(sprite->getMeshBuffer(), sprite->getRelativeTransformation());
	}	
	LOG_DEBUG("[SpriteBatcher batcher] refreshing %d meshes, %d buffers used", m_batching_mesh->getSourceBufferCount(), m_batching_mesh->getMeshBufferCount());
	m_batching_mesh->update();
	m_batching_mesh->setHardwareMappingHint(EHM_STREAM, EBT_VERTEX_AND_INDEX);

	static_cast<IMeshSceneNode*>(m_node)->setMesh(m_batching_mesh);
	LOG_INFO("[SpriteBatcher batcher] done");
	m_node->setVisible(m_sprites.size() > 0);
	m_is_dirty = false;
}

void SpriteBatcher::refresh_mesh_buffer()
{
	unsigned i = 0;
	for (auto sprite : m_sprites)
	{
		m_batching_mesh->moveMeshBuffer(i++, sprite->getRelativeTransformation());
	}
}
