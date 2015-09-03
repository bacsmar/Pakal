///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: IEntity.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
// Purpose: Interface for Entities.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IEntity.h"
#include "Component.h"
#include <vector>

//namespace Pakal
//{
//	class Component;
//
//	class _PAKALExport GenericEntity : public IEntity
//	{
//	public:	
//		virtual ~GenericEntity();
//
//		virtual BasicTaskPtr initialize() override;
//		virtual	BasicTaskPtr destroy() override;
//
//		void add_component(Component *c);
//		void remove_component(Component *c);
//		
//		Component* get_component_by_name(const std::string& name);		
//
//		template <class TComponent>
//		TComponent* get_component()
//		{ 
//			for( auto & component: m_components)
//			{
//				if( component->getType() == TComponent::getRTTI() )
//				{
//					return static_cast<TComponent*>(component);
//				}
//			}
//			return nullptr;
//		}
//
//	protected:
//		std::vector<Component*> m_components;
//	};
//
//
//}