///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: IEntity.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
// Purpose: Interface for Entities.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "IEntity.h"
#include <vector>

namespace Pakal
{
	class IComponent;

	class _PAKALExport GenericEntity : public IEntity
	{
	public:	
		virtual ~GenericEntity();

		virtual BasicTaskPtr initialize() 	override;
				void		 addComponent(IComponent *c);
				void		 removeComponent(IComponent *c);
				BasicTaskPtr initializeComponents();		
		
		IComponent* getComponentByName(const std::string& name);		

		template <class TComponent>
		TComponent* getComponent()
		{ 
			for( auto & component: m_Components)
			{
				if( component->getType() == TComponent::getRTTI() )
				{
					return static_cast<TComponent*>(component);
				}
			}
			return nullptr;
		}

	protected:
		std::vector<IComponent*> m_Components;
	};


}