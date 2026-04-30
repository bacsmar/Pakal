#include "EntityManager.h"
#include "Utils.h"
#include "Entity.h"
#include "GenericEntity.h"
#include "EntityDescriptor.h"
#include "persist/XmlReader.h"
#include "ResourceManager.h"
#include "IEntityProvider.h"

#include <vector>

using namespace Pakal;

EntityManager::EntityManager(ComponentManager* componentManager) : m_componentManager(componentManager)
{}

EntityManager::~EntityManager()
{}

const EntityDescriptor* EntityManager::get_descriptor(const std::string& name)
{
	EntityDescriptor* descriptor;

	return map_utils::try_get(m_descriptors, name, descriptor) 
		? descriptor 
		: nullptr;
}

void EntityManager::initialize()
{
	ASSERT(m_descriptors.empty());
}

void EntityManager::terminate()
{
	dispose_all_entities();
	clear_descriptors();

	for (auto& pair : m_descriptorFactories)
		delete pair.second;

	for (auto& pair : m_entityFactories)
		delete pair.second;

	m_entityFactories.clear();
	m_descriptorFactories.clear();
}

void EntityManager::load_descriptors(const std::string& source)
{
	auto stream = ResourceMgr.open_read_resource(source);

	XmlReader reader(this);
	std::vector<EntityDescriptor*> descriptors;
	reader.read(*stream, "Descriptors", descriptors);

	clear_descriptors();
	for(auto desc : descriptors)
		m_descriptors.insert(std::make_pair(desc->get_name(), desc));
}

void EntityManager::clear_descriptors()
{
	for(auto& pair : m_descriptors)
		delete pair.second;

	m_descriptors.clear();
}

void EntityManager::add_descriptor(EntityDescriptor* descriptor)
{
	m_descriptors.insert(std::make_pair(descriptor->get_name(), descriptor));
}

void EntityManager::remove_descriptor(const std::string& descriptorName)
{
	m_descriptors.erase(descriptorName);
}

void EntityManager::register_provider(IEntityProvider* provider)
{
	std::vector<IFactory<Entity>*> entity_factories;
	std::vector<IFactory<EntityDescriptor>*> descriptor_factories;

	provider->register_entities(entity_factories);
	provider->register_descriptors(descriptor_factories);

	for(auto ef : entity_factories)
	{
		add_entity_factory(ef);
	}

	for (auto df : descriptor_factories)
	{
		add_descriptor_factory(df);
	}
	
}

void EntityManager::add_entity_factory(IFactory<Entity>* factory)
{
	m_entityFactories.insert(std::make_pair(factory->get_typename(), factory));
	LOG_DEBUG("[EntityManager] registered factory for: '%s' entity type.", factory->get_typename().c_str());

}

void EntityManager::add_descriptor_factory(IFactory<EntityDescriptor>* factory)
{
	m_descriptorFactories.insert(std::make_pair(factory->get_typename(), factory));
	LOG_DEBUG("[EntityManager] registered factory for: '%s' descriptor type.", factory->get_typename().c_str());

}

Entity* EntityManager::create_entity(const std::string& name, const std::string& descriptor)
{
	IFactory<Entity>* factory = nullptr;

	if (map_utils::try_get(m_entityFactories, name, factory))
	{
		Entity* entity = factory->create();
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

void EntityManager::register_live_entity(Entity* entity)
{
	if (!entity)
	{
		return;
	}

	const uint64_t entityId = m_nextEntityId++;
	uint32_t generation = 1;
	auto generationIt = m_generations.find(entityId);
	if (generationIt != m_generations.end())
	{
		generation = generationIt->second;
	}
	m_generations[entityId] = generation;
	entity->m_handle = { entityId, generation };
	m_entitiesById[entityId] = entity;
	m_liveEntities.insert(entity);
}

void EntityManager::request_dispose(Entity* entity)
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

Entity* EntityManager::resolve(EntityHandle handle) const
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

	std::vector<Entity*> pending(m_pendingDispose.begin(), m_pendingDispose.end());
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
		auto generationIt = m_generations.find(entity->m_handle.id);
		if (generationIt != m_generations.end())
		{
			generationIt->second++;
		}
		delete entity;
		disposedCount++;
	}

	return disposedCount;
}

size_t EntityManager::dispose_all_entities()
{
	std::vector<Entity*> liveEntities(m_liveEntities.begin(), m_liveEntities.end());
	for (auto* entity : liveEntities)
	{
		request_dispose(entity);
	}

	return process_pending_disposals();
}

void* EntityManager::create_object(const std::string& className)
{
	return m_descriptorFactories.at(className)->create();
}
