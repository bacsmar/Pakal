

#pragma once

#include "Entity.h"
#include "Component.h"
#include <vector>
#include "Event.h"
#include "TaskCompletionSource.h"
#include "ComponentManager.h"
#include "RTTI.h"

namespace Pakal
{
	class Component;
	class ComponentManager;	

	class _PAKALExport GenericEntity : public Entity
	{
		DECLARE_RTTI_WITH_BASE(GenericEntity, Entity);
	public:	
		virtual ~GenericEntity();
		using Entity::Entity;

		BasicTaskPtr initialize() override;
		BasicTaskPtr terminate() override;

		void add_component(Component *c);
		void remove_component(Component *c);
		
		// Expose base class template version
		using Entity::get_component;

		template <class T>
		T* get_component() const
		{
			for (auto* component : m_components)
			{
				if (auto* typed = dynamic_cast<T*>(component))
				{
					return typed;
				}
			}
			return nullptr;
		}
		
		virtual Component* get_component(const std::string& component_id_string) const override;

		// Set the component manager (called during entity creation)
		void set_component_manager(ComponentManager* cm) { m_component_manager = cm; }

		template <class T>
		T* create_component()
		{
			if (!m_component_manager)
				return nullptr;
			
			T* ic = m_component_manager->create_component<T>();
			if (ic != nullptr)
			{
				add_component(ic);
			}
			return ic;
		}

		Event<void> evt_initialize;
		Event<void> evt_terminate;
		Event<void> evt_update;

		virtual void update();

	protected:
		std::vector<Component*> m_components;
		TaskCompletionSource<void> m_initialize_task;
		TaskCompletionSource<void> m_terminate_task;
		ComponentManager* m_component_manager = nullptr;
	};


}