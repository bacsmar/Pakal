#include "EntityDescriptor.h"
#include "persist/Archive.h"


using namespace Pakal;

EntityDescriptor::EntityDescriptor(const char* name): m_name(name)
{}

EntityDescriptor::~EntityDescriptor()
{}

void EntityDescriptor::persist(Archive* a)
{
	a->value("name", m_name);
}