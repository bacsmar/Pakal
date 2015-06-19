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
		virtual ~GenericEntity(){}

		virtual void initialize() 				override;
		virtual int addComponent(IComponent *c) override;
		virtual void initializeComponents() 	override;
		
		virtual IComponent * getComponentByName();		
		virtual IComponent * getComponentById(int id);


		template <class T>
		T * getComponent()
		{ 
			for( auto & component: m_Components)
			{
				if( component->getType() == T::getRTTI() )
				{
					return static_cast<T*>(component);
				}
			}
			return nullptr;
		}
	private:
		std::vector<IComponent*> m_Components;
	};


}