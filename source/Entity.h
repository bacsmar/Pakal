#pragma once
#include "Config.h"
#include "EntityHandle.h"
#include "TaskFwd.h"
#include "RTTI.h"

namespace Pakal
{
	class Archive;
	class Component;
	class EntityManager;

	class _PAKALExport Entity
	{
		DECLARE_RTTI(Entity);
		std::string m_descriptor;
		bool m_pendingDispose = false;
		EntityHandle m_handle;

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
		void request_dispose();
		bool is_pending_dispose() const { return m_pendingDispose; }
		EntityManager* entity_manager() const { return m_entityManager; }
		EntityHandle get_handle() const { return m_handle; }

		virtual BasicTaskPtr initialize() = 0;
		virtual BasicTaskPtr terminate() = 0;

		friend class EntityManager;
	};

	template <class T>
	T* Entity::get_component() const
	{
		Component* ic = get_component(TypeInfo::get<T>().get_name());
		return static_cast<T*>(ic);
	}
}
