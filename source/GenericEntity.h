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
#include "Event.h"
#include "TaskCompletionSource.h"
#include "ComponentManager.h"

namespace Pakal
{
	class Component;	

	class _PAKALExport GenericEntity : public IEntity
	{
	public:	
		virtual ~GenericEntity();
		using IEntity::IEntity;

		virtual BasicTaskPtr initialize() override;
		virtual	BasicTaskPtr terminate() override;

		void add_component(Component *c);
		void remove_component(Component *c);
				
		virtual Component* get_component(const std::string& component_id_string) const override;

		//template <class T>
		//T* create_component()
		//{
		//	T* ic = m_component_manager->create_component<T>();
		//	if( ic != nullptr)
		//	{
		//		add_component(ic);
		//	}
		//	return ic;
		//}

		Event<void> evt_initialize;
		Event<void> evt_terminate;
		Event<void> evt_update;

		virtual void update();

	protected:
			std::vector<Component*> m_components;
			TaskCompletionSource<void> m_initialize_task;
			TaskCompletionSource<void> m_terminate_task;
	};


}