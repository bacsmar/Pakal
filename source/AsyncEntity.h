// The asynchronous entity model: an entity's components (AsyncComponent) are made by the systems (graphics,
// physics...) and set up asynchronously (tasks); each system updates its own components on its own thread and at its
// own rate. For a game's simulation in fixed, ordered steps use SyncEntity (SyncEntity.h); the simulation never reads an
// AsyncEntity. See docs/ARCHITECTURE.md.
#pragma once
#include "Config.h"
#include "EntityHandle.h"
#include "TaskFwd.h"
#include "RTTI.h"

namespace Pakal
{
	class Archive;
	class AsyncComponent;
	class EntityManager;

	class _PAKALExport AsyncEntity
	{
		DECLARE_RTTI(AsyncEntity);
		std::string m_descriptor;
		bool m_pendingDispose = false;
		EntityHandle m_handle;

	protected:		
		EntityManager* const m_entityManager;
	public:
		explicit AsyncEntity(EntityManager* m);

		virtual ~AsyncEntity();

		virtual void persist(Archive* a);

		template <class T> T* get_component() const;
		virtual AsyncComponent* get_component(const std::string& componentName) const;

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
	T* AsyncEntity::get_component() const
	{
		AsyncComponent* ic = get_component(TypeInfo::get<T>().get_name());
		return static_cast<T*>(ic);
	}
}
