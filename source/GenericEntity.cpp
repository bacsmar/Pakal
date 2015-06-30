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
#include "RTTI.h"
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
	void GenericEntity::initializeComponents()
	{
		for( auto & component: m_Components)
		{
			component->setParentEntity(this);			
			BasicTask * task = component->init();
			//vectorDeInicializacion.push_back( task );
		}
		//return TaskUtils::whenAll( vectorDeInicializacion );
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