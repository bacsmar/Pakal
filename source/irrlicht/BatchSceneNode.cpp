#include "BatchSceneNode.h"

using namespace irr;

/*Pakal::BatchSceneNode::BatchSceneNode(irr::scene::IMesh* mesh, 
	ISceneNode* parent, 
	irr::scene::ISceneManager* mgr, 
	irr::s32 id): 
	CMeshSceneNode(mesh, parent, mgr, id) 
{
}
*/
void Pakal::BatchSceneNode::setVisible(bool isVisible)
{
}

void Pakal::BatchSceneNode::remove()
{
}

void Pakal::BatchSceneNode::setScale(const irr::core::vector3df& scale)
{
}

void Pakal::BatchSceneNode::setRotation(const irr::core::vector3df& rotation)
{
}

void Pakal::BatchSceneNode::setPosition(const irr::core::vector3df& newpos)
{
}

void Pakal::BatchSceneNode::setParent(ISceneNode* newParent)
{
}

void Pakal::BatchSceneNode::setMesh(irr::scene::IMesh* mesh)
{
}

void Pakal::BatchSceneNode::render()
{
}

const irr::core::aabbox3d<float>& Pakal::BatchSceneNode::getBoundingBox() const
{
	return{};
}

irr::scene::IMesh* Pakal::BatchSceneNode::getMesh()
{
	return nullptr;
}

irr::scene::IShadowVolumeSceneNode* Pakal::BatchSceneNode::addShadowVolumeSceneNode(const irr::scene::IMesh* shadowMesh, irr::s32 id, bool zfailmethod, irr::f32 infinity)
{
	return  nullptr;
}

void Pakal::BatchSceneNode::setReadOnlyMaterials(bool readonly)
{
}

bool Pakal::BatchSceneNode::isReadOnlyMaterials() const
{
	return true;
}
