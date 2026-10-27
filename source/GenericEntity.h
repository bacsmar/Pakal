

#pragma once

#include "AsyncEntity.h"
#include "AsyncComponent.h"
#include <vector>
#include "Event.h"
#include "TaskCompletionSource.h"
#include "ComponentManager.h"
#include "RTTI.h"

namespace Pakal
{
	class AsyncComponent;
	class ComponentManager;	

	class _PAKALExport GenericEntity : public AsyncEntity
	{
		DECLARE_RTTI_WITH_BASE(GenericEntity, AsyncEntity);
	public:	
		virtual ~GenericEntity();
		using AsyncEntity::AsyncEntity;

		BasicTaskPtr initialize() override;
		BasicTaskPtr terminate() override;

		void add_component(AsyncComponent *c);
		void remove_component(AsyncComponent *c);
		bool destroy_component(AsyncComponent* c);
		const std::vector<AsyncComponent*>& get_components() const { return m_components; }
		
		// Expose base class template version
		using AsyncEntity::get_component;

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
		
		virtual AsyncComponent* get_component(const std::string& component_id_string) const override;

		// Set the component manager (called during entity creation)
		void set_component_manager(ComponentManager* cm) { m_component_manager = cm; }
		ComponentManager* get_component_manager() const { return m_component_manager; }

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
		std::vector<AsyncComponent*> m_components;
		TaskCompletionSource<void> m_initialize_task;
		TaskCompletionSource<void> m_terminate_task;
		ComponentManager* m_component_manager = nullptr;
	};


}