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
}

void Pakal::MeshComponent::onDestroy()
{
	if (m_node)
	{
		m_node->remove();
	}
}

Pakal::MeshComponent::~MeshComponent()
{
}

Pakal::MeshComponent::MeshComponent(IrrGraphicsSystem* irr): GraphicComponent(irr), m_node(nullptr)
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
		if ( m_node )
		{
			m_node->remove();
		}
		m_node = getSystem()->getSmgr()->addMeshSceneNode(m_mesh);
	}, m_GraphicSystem->threadId());
}

Pakal::BasicTaskPtr Pakal::MeshComponent::LoadTextureAsync(const std::string& textureName)
{
	return m_GraphicSystem->getScheduler()->executeInThread([=]()
	{
		m_texture = getSystem()->getDriver()->getTexture(textureName.c_str());

		m_node->setMaterialFlag(EMF_LIGHTING, false);
		m_node->setMaterialTexture(0, m_texture);
		m_node->setVisible(true);
	}, m_GraphicSystem->threadId());
}

void Pakal::MeshComponent::setPosition(const Pakal::core::vector3df& position)
{
	irr::core::vector3df v(position.X, position.Y, position.Z);
	m_node->setPosition( v );
}

Pakal::core::vector3df Pakal::MeshComponent::getPosition()
{
	const vector3df vector3D = m_node->getPosition();

	return core::vector3df(vector3D.X,vector3D.Y,vector3D.Z);
}