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

		struct BodyPart
		{
			float density = 1.0;
			float friction = 0.0;;
			float restitution = 0.5;
			bool is_sensor = false;
			unsigned id = -1;

			BodyPart(float _density, float _friction, float _restitution, bool isSensor, unsigned _id) :
				density(_density), friction(_friction),
				restitution(_restitution), is_sensor(isSensor), id(_id)
			{}
			std::string type;
		};

		// internal use only
		bool on_collide(const PhysicComponent& other)
		{
			auto otherEntity = other.get_parent_entity();
			ASSERT(otherEntity && this->get_parent_entity());
			{
				event_collide.notify(otherEntity);
			}
			return true;
		}
		// internal use only
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

