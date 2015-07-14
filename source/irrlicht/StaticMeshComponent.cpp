#pragma once
#include "StaticMeshComponent.h"

void Pakal::StaticMeshComponent::onInit()
{
	m_modelName = "sphere.irrmesh";
	m_textureName = "concretewall.jpg";

	IrrGraphicsSystem* irrGraphicsSystem = static_cast<IrrGraphicsSystem*>(m_GraphicSystem);

	m_mesh = irrGraphicsSystem->getSmgr()->getMesh(m_modelName.c_str());	
	m_node = irrGraphicsSystem->getSmgr()->addMeshSceneNode( m_mesh );
	
	m_texture = irrGraphicsSystem->getDriver()->getTexture(m_textureName.c_str());

	if( m_node)
	{
		m_node->setMaterialFlag(irr::video::EMF_LIGHTING, false);
		m_node->setMaterialTexture( 0,  m_texture );
	}
}

void Pakal::StaticMeshComponent::onDestroy()
{	
	m_node->remove();
	m_node = nullptr;
}

void Pakal::StaticMeshComponent::onUpdate()
{
	m_node->setPosition( irr::core::vector3df(0,0,0) );	
}

void Pakal::StaticMeshComponent::setModel(const char* modelName)
{
	m_modelName = modelName;
	notify();
}

void Pakal::StaticMeshComponent::setTexure(const char* textureName)
{
	m_textureName = textureName;
	notify();
}

void Pakal::StaticMeshComponent::setPosition(float x, float y, float z)
{	
	//m_position = values;
	notify();
}

void Pakal::StaticMeshComponent::getPosition()
{
	m_node->getPosition();
}
