///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: IEntity.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
// Purpose: Interface for Entities.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "GenericEntity.h"
#include "Component.h"
#include "Task.h"
#include <string>

//namespace Pakal
//{
//
//	BasicTaskPtr GenericEntity::destroy()
//	{
//		std::vector<BasicTaskPtr> destroyTasks;
//
//		for( auto & component: m_components)
//		{			
//			destroyTasks.push_back(component->destroy());
//		}
//
//		return TaskUtils::when_all(destroyTasks);
//	}
//
//	///////////////////////////////////////////////////////////////////////////////////////////////////////
//	void GenericEntity::add_component(Component *c) 
//	{
//		m_components.push_back(c);
//	}
//
//	void GenericEntity::remove_component(Component* c)
//	{
//		m_components.erase(find(m_components.begin(),m_components.end(),c));
//	}
//
//	///////////////////////////////////////////////////////////////////////////////////////////////////////
//	BasicTaskPtr GenericEntity::initialize()
//	{
//		std::vector<BasicTaskPtr> initTasks;
//
//		for (auto c : m_components)
//		{
//			c->set_parent_entity(this);
//			initTasks.push_back(c->initialize());
//		}
//
//		return TaskUtils::when_all(initTasks);
//	}
//
//	///////////////////////////////////////////////////////////////////////////////////////////////////////
//	GenericEntity::~GenericEntity() 
//	{
//		
//	}
//
//
//	///////////////////////////////////////////////////////////////////////////////////////////////////////
//	Component * GenericEntity::get_component_by_name(const std::string& name)
//	{
//		for( auto & component: m_components)
//		{
//			if (component->getType().getName() == name)
//				return component;
//		}
//		return nullptr;
//	}	
//	///////////////////////////////////////////////////////////////////////////////////////////////////////
//
//}