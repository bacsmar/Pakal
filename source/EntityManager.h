#pragma once
#include "Config.h"
#include "EntityHandle.h"
#include "IManager.h"
#include "RTTI.h"
#include "persist/PersistUtils.h"
#include "ComponentManager.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace Pakal
{
	class Entity;
	class IEntityProvider;
	class EntityDescriptor;

	class _PAKALExport EntityManager : public IManager, public IFactoryManager
	{
		ComponentManager* m_componentManager;
		std::unordered_map<std::string, EntityDescriptor*> m_descriptors;
		std::unordered_map<std::string, IFactory<Entity>*> m_entityFactories;
		std::unordered_map<std::string, IFactory<EntityDescriptor>*> m_descriptorFactories;
		std::unordered_set<Entity*> m_liveEntities;
		std::unordered_set<Entity*> m_pendingDispose;
		std::unordered_map<uint64_t, Entity*> m_entitiesById;
		std::unordered_map<uint64_t, uint32_t> m_generations;
		uint64_t m_nextEntityId = 1;

		void* create_object(const std::string& className) override;

	public:
		explicit EntityManager(ComponentManager* componentManager);
		virtual ~EntityManager();

		ComponentManager* get_component_manager() { return m_componentManager; };

		const EntityDescriptor* get_descriptor(const std::string& name);
		template<class TDescriptor>
		const TDescriptor* get_descriptor(const std::string& name);

		void initialize() override;
		void terminate() override;

		void load_descriptors(const std::string& source);
		void clear_descriptors();

		void add_descriptor(EntityDescriptor* descriptor);
		void remove_descriptor(const std::string& descriptor);

		void register_provider(IEntityProvider* provider);
		void add_entity_factory(IFactory<Entity>* factory);
		void add_descriptor_factory(IFactory<EntityDescriptor>* factory);


		Entity* create_entity(const std::string& name, const std::string& descriptor = "");
		void request_dispose(Entity* entity);
		void request_dispose(EntityHandle handle);
		size_t process_pending_disposals();
		size_t dispose_all_entities();
		Entity* resolve(EntityHandle handle) const;
		template<class TEntity>
		TEntity* resolve(EntityHandle handle) const;
		template<class TEntity>
		TEntity* create_entity(const std::string& descriptor = "");

		void register_live_entity(Entity* entity);

	};

	template <class TDescriptor>
	const TDescriptor* EntityManager::get_descriptor(const std::string& name)
	{
		return static_cast<const TDescriptor*>(get_descriptor(name));
	}

	template <class TEntity>
	TEntity* EntityManager::create_entity(const std::string& descriptor)
	{
		Entity* entity = create_entity(TypeInfo::get<TEntity>().get_name(),descriptor);
		return static_cast<TEntity*>(entity);
	}

	template <class TEntity>
	TEntity* EntityManager::resolve(EntityHandle handle) const
	{
		return static_cast<TEntity*>(resolve(handle));
	}
}
