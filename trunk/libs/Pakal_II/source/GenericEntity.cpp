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
		auto tasks = 
				from(m_components)
			>>  select([this](IComponent* c){ c->set_parent_entity(this);  return c->init(); })
			>>  to_vector(m_components.size());

		return TaskUtils::when_all(tasks);
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