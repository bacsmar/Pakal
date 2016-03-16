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
#include "RTTI.h"

namespace Pakal
{
	class ComponentManager;
	class Component;

	class _PAKALExport IEntity
	{
		friend class EntityManager;
	protected:
		virtual ~IEntity() {}
		virtual BasicTaskPtr initialize() = 0;
		virtual BasicTaskPtr terminate() = 0;

		Pakal::ComponentManager*	m_component_manager;

	public:
		// ctor
		explicit IEntity(ComponentManager* c) :m_component_manager(c) {}

		virtual Component* get_component(const std::string& component_id_string) const { return nullptr; };

		template <class T>
		T* get_component() const
		{
			Component* ic = get_component(TypeInfo::get<T>().get_name());
			ASSERT(!ic || (ic && ic->get_type().is_derived_from<T>()));
			//ASSERT(ic->getType() == TypeInfo::get<T>());
			return static_cast<T*>(ic);
		}
	};


}