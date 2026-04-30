#include "Entity.h"
#include "EntityManager.h"
#include "persist/Archive.h"


using namespace Pakal;

Entity::Entity(EntityManager* m) : m_entityManager(m)
{}

Entity::~Entity()
{}

void Entity::request_dispose()
{
	if (m_pendingDispose || !m_entityManager)
	{
		return;
	}

	m_entityManager->request_dispose(this);
}

void Entity::set_descriptor(const std::string& descriptor)
{
	m_descriptor = descriptor;
}
const std::string& Entity::get_descriptor() const
{
	return m_descriptor;
}

void Entity::persist(Archive* a)
{
	a->value("descriptor", m_descriptor);
}

Component* Entity::get_component(const std::string& componentName) const
{
	return nullptr;
}