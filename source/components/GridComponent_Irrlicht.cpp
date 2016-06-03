#include "GridComponent_Irrlicht.h"

Pakal::GridComponent_Irrlicht::GridComponent_Irrlicht(IrrGraphicsSystem* irrGraphicsSystem): m_system(irrGraphicsSystem), m_node(nullptr)
{
}

Pakal::GridComponent_Irrlicht::~GridComponent_Irrlicht()
{
}

void Pakal::GridComponent_Irrlicht::show()
{
}

void Pakal::GridComponent_Irrlicht::hide()
{
}

Pakal::BasicTaskPtr Pakal::GridComponent_Irrlicht::initialize(const Settings& settings)
{
	return m_system->execute_block([=]()
	{
		ASSERT(m_node == nullptr);		
		m_node = new CGridSceneNode(m_system->get_smgr()->getRootSceneNode(), m_system->get_smgr());
		m_node->drop();
	});
}

Pakal::BasicTaskPtr Pakal::GridComponent_Irrlicht::destroy()
{
	return m_system->execute_block([=]()
	{
		m_node->remove();
		m_node = nullptr;
	});
}
