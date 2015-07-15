#pragma once
#include "irrlicht/IrrGraphicsSystem.h"

#include "MeshComponent.h"
#include "EventScheduler.h"
#include "TaskFwd.h"


using namespace irr::core;
using namespace irr::scene;
using namespace irr::video;

void Pakal::MeshComponent::onInit()
{
	vector3df zero(0, 0, 0);

	m_node = getSystem()
		->getSmgr()
		->addMeshSceneNode(nullptr, nullptr, -1, zero, zero, zero, true);
}

void Pakal::MeshComponent::onDestroy()
{
	m_node->remove();
}

Pakal::MeshComponent::~MeshComponent()
{
}

Pakal::MeshComponent::MeshComponent(IrrGraphicsSystem* irr): GraphicComponent(irr)
{
	
}

Pakal::IrrGraphicsSystem* Pakal::MeshComponent::getSystem()
{
	return static_cast<IrrGraphicsSystem*>(m_GraphicSystem);
}

Pakal::BasicTaskPtr Pakal::MeshComponent::LoadMeshAsync(const std::string& meshName)
{
	return m_GraphicSystem->getScheduler()->executeInThread([=]()
	{
		m_mesh = getSystem()->getSmgr()->getMesh(meshName.c_str());
		m_node->setMesh(m_mesh);
	}, m_GraphicSystem->threadId());
}

Pakal::BasicTaskPtr Pakal::MeshComponent::LoadTextureAsync(const std::string& textureName)
{
	return m_GraphicSystem->getScheduler()->executeInThread([=]()
	{
		m_texture = getSystem()->getDriver()->getTexture(textureName.c_str());

		m_node->setMaterialFlag(EMF_LIGHTING, false);
		m_node->setMaterialTexture(0, m_texture);
	}, m_GraphicSystem->threadId());
}

void Pakal::MeshComponent::setPosition(const vector3df& position)
{
	m_node->setPosition(position);
}

const vector3df& Pakal::MeshComponent::getPosition()
{
	return m_node->getPosition();
}