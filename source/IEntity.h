///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: IEntity.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
// Purpose: Interface for Entities.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Config.h"
#include "TaskFwd.h"

namespace Pakal
{
	class Component;

	class _PAKALExport IEntity
	{
	protected:	
		virtual ~IEntity(){}
		virtual BasicTaskPtr initialize() = 0;
		virtual BasicTaskPtr terminate() = 0;		
	public:
		virtual void on_collide(const IEntity& other) = 0;
		virtual Component* get_component(const char* component_typename) const = 0;

		template <class T>
		T* get_component() const
		{
			Component* ic = get_component(TypeInfo::get<T>().getName());
			ASSERT( !ic || (ic && ic->get_type().is_derived_from<T>()) );
			//ASSERT(ic->getType() == TypeInfo::get<T>());
			return static_cast<T*>(ic);
		}
	};

	
}