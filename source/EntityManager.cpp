#include "EntityManager.h"
#include "Utils.h"
#include "AsyncEntity.h"
#include "GenericEntity.h"
#include "LogMgr.h"

#include <vector>

using namespace Pakal;

EntityManager::EntityManager(ComponentManager* componentManager) : m_componentManager(componentManager)
{}

EntityManager::~EntityManager()
{}

void EntityManager::initialize()
{
}

void EntityManager::terminate()
{
	dispose_all_entities();

	for (auto& pair : m_entityFactories)
		delete pair.second;

	m_entityFactories.clear();
}

void EntityManager::add_entity_factory(IFactory<AsyncEntity>* factory)
{
	// the manager owns its factories: one for a type already registered is not kept (the first stays), and is deleted
	// instead of leaked
	if (!m_entityFactories.insert(std::make_pair(factory->get_typename(), factory)).second)
	{
		LOG_WARNING("[EntityManager] a factory for '%s' is already registered: the new one is discarded", factory->get_typename().c_str());
		delete factory;
		return;
	}
	LOG_DEBUG("[EntityManager] registered factory for: '%s' entity type.", factory->get_typename().c_str());
}

AsyncEntity* EntityManager::create_entity(const std::string& name, const std::string& descriptor)
{
	IFactory<AsyncEntity>* factory = nullptr;

	if (map_utils::try_get(m_entityFactories, name, factory))
	{
		AsyncEntity* entity = factory->create();
		if (!entity)
		{
			LOG_ERROR("[EntityManager] the factory of '%s' made no entity", name.c_str());
			return nullptr;
		}
		entity->set_descriptor(descriptor);
		register_live_entity(entity);
		
		// Set ComponentManager on GenericEntity if applicable
		GenericEntity* genericEntity = dynamic_cast<GenericEntity*>(entity);
		if (genericEntity && m_componentManager)
		{
			genericEntity->set_component_manager(m_componentManager);
		}
		
		return entity;
	}
	
	return nullptr;
}

void EntityManager::register_live_entity(AsyncEntity* entity)
{
	if (!entity)
	{
		return;
	}

	// ids are never reused, so a disposed entity's handle no longer resolves (its id is gone); the generation stays in
	// the handle for when slots are reused
	const uint64_t entityId = m_nextEntityId++;
	const uint32_t generation = 1;
	m_generations[entityId] = generation;
	entity->m_handle = { entityId, generation };
	m_entitiesById[entityId] = entity;
	m_liveEntities.insert(entity);
}

void EntityManager::request_dispose(AsyncEntity* entity)
{
	if (!entity || entity->m_pendingDispose)
	{
		return;
	}

	entity->m_pendingDispose = true;
	m_pendingDispose.insert(entity);
}

void EntityManager::request_dispose(EntityHandle handle)
{
	request_dispose(resolve(handle));
}

AsyncEntity* EntityManager::resolve(EntityHandle handle) const
{
	if (!handle.is_valid())
	{
		return nullptr;
	}

	auto generationIt = m_generations.find(handle.id);
	if (generationIt == m_generations.end() || generationIt->second != handle.generation)
	{
		return nullptr;
	}

	auto entityIt = m_entitiesById.find(handle.id);
	if (entityIt == m_entitiesById.end())
	{
		return nullptr;
	}

	return entityIt->second;
}

size_t EntityManager::process_pending_disposals()
{
	m_componentManager->process_pending_disposals();

	if (m_pendingDispose.empty())
	{
		return 0;
	}

	std::vector<AsyncEntity*> pending(m_pendingDispose.begin(), m_pendingDispose.end());
	m_pendingDispose.clear();

	size_t disposedCount = 0;
	for (auto* entity : pending)
	{
		if (!entity)
		{
			continue;
		}

		auto* genericEntity = dynamic_cast<GenericEntity*>(entity);
		if (genericEntity)
		{
			const auto components = genericEntity->get_components();
			for (auto* component : components)
			{
				component->prepare_dispose();
			}
		}

		m_liveEntities.erase(entity);
		m_entitiesById.erase(entity->m_handle.id);
		// forgotten, not kept with a higher generation: the map grew by one entry for every entity ever made
		m_generations.erase(entity->m_handle.id);
		delete entity;
		disposedCount++;
	}

	return disposedCount;
}

size_t EntityManager::dispose_all_entities()
{
	std::vector<AsyncEntity*> liveEntities(m_liveEntities.begin(), m_liveEntities.end());
	for (auto* entity : liveEntities)
	{
		request_dispose(entity);
	}

	return process_pending_disposals();
}
