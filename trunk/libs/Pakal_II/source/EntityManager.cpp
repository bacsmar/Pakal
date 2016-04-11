#include "EntityManager.h"
#include "Utils.h"
#include "Entity.h"
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

	clear_descriptors();
	reader.read(*stream, "Descriptors", m_descriptors);
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
		return entity;
	}
	
	return nullptr;
}

void* EntityManager::create_object(const std::string& className)
{
	return m_descriptorFactories.at(className)->create();
}
