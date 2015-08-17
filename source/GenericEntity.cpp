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
		return initialize_components();
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	void GenericEntity::add_component(IComponent *c) 
	{
		m_components.push_back(c);
	}

	void GenericEntity::remove_component(IComponent* c)
	{
		m_components.erase(find(m_components.begin(),m_components.end(),c));
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////
	BasicTaskPtr GenericEntity::initialize_components()
	{
		std::vector<BasicTaskPtr> terminationTasks;

		for (auto c : m_components)
		{
			c->set_parent_entity(this);
			terminationTasks.push_back(c->init());
		}

		return TaskUtils::when_all(terminationTasks);
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////
	GenericEntity::~GenericEntity() 
	{
		for( auto & component: m_components)
		{			
			component->destroy();
		}
	}


	///////////////////////////////////////////////////////////////////////////////////////////////////////
	IComponent * GenericEntity::get_component_by_name(const std::string& name)
	{
		for( auto & component: m_components)
		{
			if (component->getType().getName() == name)
				return component;
		}
		return nullptr;
	}	
	///////////////////////////////////////////////////////////////////////////////////////////////////////

}