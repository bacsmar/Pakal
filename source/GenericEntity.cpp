///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: IEntity.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
// Purpose: Interface for Entities.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "GenericEntity.h"
#include "IComponent.h"
#include "Task.h"
#include <string>

namespace Pakal
{
	BasicTaskPtr GenericEntity::initialize()
	{
		return initializeComponents();
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	void GenericEntity::addComponent(IComponent *c) 
	{
		m_Components.push_back(c);
	}

	void GenericEntity::removeComponent(IComponent* c)
	{
		m_Components.erase(find(m_Components.begin(),m_Components.end(),c));
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////
	BasicTaskPtr GenericEntity::initializeComponents()
	{
		std::vector<BasicTaskPtr> initializationTaskVector;

		for( auto & component: m_Components)
		{
			if (!component->isInitialized())
			{
				component->setParentEntity(this);
				initializationTaskVector.push_back(component->init());
			}
		}
		return TaskUtils::whenAll(initializationTaskVector);
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////
	GenericEntity::~GenericEntity() 
	{
		for( auto & component: m_Components)
		{			
			component->destroy();
		}
	}


	///////////////////////////////////////////////////////////////////////////////////////////////////////
	IComponent * GenericEntity::getComponentByName(const std::string& name)
	{
		for( auto & component: m_Components)
		{
			if (component->getType().getName() == name)
				return component;
		}
		return nullptr;
	}	
	///////////////////////////////////////////////////////////////////////////////////////////////////////

}