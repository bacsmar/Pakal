

#include "GenericEntity.h"
#include "Component.h"
#include "Task.h"
#include <string>

namespace Pakal
{

	BasicTaskPtr GenericEntity::terminate()
	{
		evt_terminate.notify();
		// TODO: lo mismo que para initialize
		m_terminate_task.set_completed();
		return m_terminate_task.get_task();
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////
	void GenericEntity::add_component(Component *c) 
	{
		c->set_parent_entity(this);
		m_components.push_back(c);
	}

	void GenericEntity::remove_component(Component* c)
	{
		c->set_parent_entity(nullptr);
		m_components.erase(find(m_components.begin(),m_components.end(),c));
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////
	BasicTaskPtr GenericEntity::initialize()
	{
		//for (auto c : m_components)
		//{
		//	c->set_parent_entity(this);
		//}

		evt_initialize.notify();
		// TODO: pensar como completar este task de inicializacion...
		// podria ser pasando un parametro en el evento...
		m_initialize_task.set_completed();
		return m_initialize_task.get_task();
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	GenericEntity::~GenericEntity() 
	{
		for (auto & component : m_components)
		{
			delete component;
		}
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	Component* GenericEntity::get_component(const std::string& component_id_string) const
	{
		for (auto & component : m_components)
		{
			if (component->get_type().get_name() == component_id_string)
				return component;
		}
		return nullptr;
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	void GenericEntity::update()
	{
		evt_update.notify();
	}

	///////////////////////////////////////////////////////////////////////////////////////////////////////

}