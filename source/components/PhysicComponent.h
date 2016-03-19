///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: IComponent.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: 26-11-2015
// Purpose: Component physics.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "config.h"
#include "Component.h"
#include "Event.h"

namespace Pakal
{	
	class IEntity;

	class _PAKALExport PhysicComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(PhysicComponent, Component);		
	public:		
		virtual ~PhysicComponent(){}

		Event<const IEntity*> event_collide;
		Event<const IEntity*> event_end_collide;

		// internal use only
		bool on_collide(const PhysicComponent& other)
		{
			auto otherEntity = other.get_parent_entity();
			//auto thisEntity = this->get_parent_entity();
			ASSERT(otherEntity && this->get_parent_entity());
			{
				event_collide.notify(otherEntity);
			}
			return true;
		}

		bool on_end_collide(const PhysicComponent& other)
		{
			auto otherEntity = other.get_parent_entity();			
			ASSERT(otherEntity && this->get_parent_entity());
			{
				event_end_collide.notify(otherEntity);
			}
			return true;
		}
	};
}

