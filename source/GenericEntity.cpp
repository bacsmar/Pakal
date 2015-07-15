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

namespace Pakal
{
	void GenericEntity::initialize()
	{
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	int GenericEntity::addComponent(IComponent *c) 
	{
		m_Components.push_back(c);
		return m_Components.size();
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	BasicTaskPtr GenericEntity::initializeComponents()
	{
		std::vector<BasicTaskPtr> initializationTaskVector;

		for( auto & component: m_Components)
		{
			component->setParentEntity(this);
			BasicTaskPtr task = component->init();

			initializationTaskVector.push_back( task );
		}

		BasicTaskPtr onFinish = TaskUtils::whenAll( initializationTaskVector );
		return onFinish;
	}	
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	GenericEntity::~GenericEntity() 
	{
		//for( auto & component: m_Components)
		//{			
		//	component->destroy();
		//}
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	IComponent * GenericEntity::getComponentByName()
	{
		for( auto & component: m_Components)
		{
			component->getType();
		}
		return nullptr;
	}	
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	IComponent * GenericEntity::getComponentById(int id)
	{
		return m_Components[id];
	}
}