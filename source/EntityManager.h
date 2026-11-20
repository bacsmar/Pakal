// Makes and keeps the AsyncEntities (see AsyncEntity.h and docs/ARCHITECTURE.md): what a game draws and hears, never its
// simulation (that is a SimWorld's). Entities are made by type name from the factories registered here
// (Pakal::GenericEntity by the engine).
#pragma once
#include "Config.h"
#include "EntityHandle.h"
#include "IManager.h"
#include "RTTI.h"
#include "ComponentManager.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace Pakal
{
	class AsyncEntity;

	class _PAKALExport EntityManager : public IManager
	{
		ComponentManager* m_componentManager;
		std::unordered_map<std::string, IFactory<AsyncEntity>*> m_entityFactories;
		std::unordered_set<AsyncEntity*> m_liveEntities;
		std::unordered_set<AsyncEntity*> m_pendingDispose;
		std::unordered_map<uint64_t, AsyncEntity*> m_entitiesById;
		std::unordered_map<uint64_t, uint32_t> m_generations;
		uint64_t m_nextEntityId = 1;

	public:
		explicit EntityManager(ComponentManager* componentManager);
		virtual ~EntityManager();

		ComponentManager* get_component_manager() { return m_componentManager; };

		void initialize() override;
		void terminate() override;

		void add_entity_factory(IFactory<AsyncEntity>* factory);


		AsyncEntity* create_entity(const std::string& name, const std::string& descriptor = "");
		void request_dispose(AsyncEntity* entity);
		void request_dispose(EntityHandle handle);
		size_t process_pending_disposals();
		size_t dispose_all_entities();
		AsyncEntity* resolve(EntityHandle handle) const;
		template<class TEntity>
		TEntity* resolve(EntityHandle handle) const;
		template<class TEntity>
		TEntity* create_entity(const std::string& descriptor = "");

		void register_live_entity(AsyncEntity* entity);

	};

	template <class TEntity>
	TEntity* EntityManager::create_entity(const std::string& descriptor)
	{
		AsyncEntity* entity = create_entity(TypeInfo::get<TEntity>().get_name(),descriptor);
		return static_cast<TEntity*>(entity);
	}

	template <class TEntity>
	TEntity* EntityManager::resolve(EntityHandle handle) const
	{
		return static_cast<TEntity*>(resolve(handle));
	}
}
