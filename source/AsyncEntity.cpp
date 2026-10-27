#include "AsyncEntity.h"
#include "EntityManager.h"
#include "persist/Archive.h"


using namespace Pakal;

AsyncEntity::AsyncEntity(EntityManager* m) : m_entityManager(m)
{}

AsyncEntity::~AsyncEntity()
{}

void AsyncEntity::request_dispose()
{
	if (m_pendingDispose || !m_entityManager)
	{
		return;
	}

	m_entityManager->request_dispose(this);
}

void AsyncEntity::set_descriptor(const std::string& descriptor)
{
	m_descriptor = descriptor;
}
const std::string& AsyncEntity::get_descriptor() const
{
	return m_descriptor;
}

void AsyncEntity::persist(Archive* a)
{
	a->value("descriptor", m_descriptor);
}

AsyncComponent* AsyncEntity::get_component(const std::string& componentName) const
{
	return nullptr;
}