#pragma once
#include "Config.h"
#include "TaskFwd.h"
#include "RTTI.h"

namespace Pakal
{
	class Archive;
	class Component;
	class EntityManager;

	class _PAKALExport Entity
	{
		std::string m_descriptor;

	protected:		
		EntityManager* const m_entityManager;
	public:
		explicit Entity(EntityManager* m);

		virtual ~Entity();

		virtual void persist(Archive* a);

		template <class T> T* get_component() const;
		virtual Component* get_component(const std::string& componentName) const;

		void set_descriptor(const std::string& descriptor);
		const std::string& get_descriptor() const;

		virtual BasicTaskPtr initialize() = 0;
		virtual BasicTaskPtr terminate() = 0;
	};

	template <class T>
	T* Entity::get_component() const
	{
		Component* ic = get_component(TypeInfo::get<T>().get_name());
		return static_cast<T*>(ic);
	}
}
