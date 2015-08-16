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
#include <cpplinq.hpp>

using namespace cpplinq;

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
		auto tasks = 
				from(m_Components)
			>>  select([this](IComponent* c){ c->setParentEntity(this);  return c->init(); })
			>>  to_vector(m_Components.size());

		return TaskUtils::whenAll(tasks);
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