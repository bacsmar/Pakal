#include "Entity.h"
#include "persist/Archive.h"


using namespace Pakal;

Entity::Entity(EntityManager* m) : m_entityManager(m)
{}

Entity::~Entity()
{}

void Entity::set_descriptor(const std::string& descriptor)
{
	m_descriptor = descriptor;
}
const std::string& Entity::get_descriptor()
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