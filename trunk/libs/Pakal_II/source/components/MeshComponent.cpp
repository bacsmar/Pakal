#pragma once
#include "irrlicht/IrrGraphicsSystem.h"

#include "MeshComponent.h"
#include "EventScheduler.h"
#include "TaskFwd.h"


using namespace irr::core;
using namespace irr::scene;
using namespace irr::video;

void Pakal::MeshComponent::on_init()
{
}

void Pakal::MeshComponent::on_destroy()
{
	if (m_node)
	{
		m_node->remove();
	}

	delete this;
}

Pakal::MeshComponent::~MeshComponent()
{
}

Pakal::MeshComponent::MeshComponent(IrrGraphicsSystem* irr): GraphicComponent(irr), m_node(nullptr)
{
}

Pakal::IrrGraphicsSystem* Pakal::MeshComponent::getSystem()
{
	return static_cast<IrrGraphicsSystem*>(m_graphic_system);
}

Pakal::BasicTaskPtr Pakal::MeshComponent::LoadMeshAsync(const std::string& meshName)
{
	return m_graphic_system->get_scheduler()->execute_in_thread([=]()
	{
		m_mesh = getSystem()->get_smgr()->getMesh(meshName.c_str());
		if ( m_node )
		{
			m_node->remove();
		}
		m_node = getSystem()->get_smgr()->addMeshSceneNode(m_mesh);
	}, m_graphic_system->get_thread_id());

}

Pakal::BasicTaskPtr Pakal::MeshComponent::LoadTextureAsync(const std::string& textureName)
{
	return m_graphic_system->get_scheduler()->execute_in_thread([=]()
	{
		m_texture = getSystem()->get_driver()->getTexture(textureName.c_str());

		m_node->setMaterialFlag(EMF_LIGHTING, false);
		m_node->setMaterialTexture(0, m_texture);
		m_node->setVisible(true);
	}, m_graphic_system->get_thread_id());
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