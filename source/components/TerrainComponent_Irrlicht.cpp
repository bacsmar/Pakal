#include "TerrainComponent_Irrlicht.h"
#include <irrlicht/StreamIrrlitch.h>

Pakal::TerrainComponent_Irrlicht::TerrainComponent_Irrlicht(IrrGraphicsSystem* irrGraphicsSystem): m_system(irrGraphicsSystem), m_node(nullptr)
{
}

Pakal::TerrainComponent_Irrlicht::~TerrainComponent_Irrlicht()
{
}

void Pakal::TerrainComponent_Irrlicht::show()
{
}

void Pakal::TerrainComponent_Irrlicht::hide()
{
}

Pakal::BasicTaskPtr Pakal::TerrainComponent_Irrlicht::initialize(const Settings& settings)
{
	return m_system->execute_block([=]()
	{
		ASSERT(m_node == nullptr);
		auto driver = m_system->get_driver();
		//auto terrainFile = ResourceMgr.open_read_resource(settings.terrain_file_name);
		//m_node = m_system->get_smgr()->addTerrainSceneNode(new StreamIrrlitch(terrainFile));
		m_node = m_system->get_smgr()->addTerrainSceneNode(settings.terrain_file_name.c_str());

		//m_node->setScale({ 0.5f, 0.5f, 0.5f });
		
		m_node->setMaterialFlag(irr::video::EMF_LIGHTING, false);
		//m_node->setMaterialFlag(irr::video::EMF_WIREFRAME, true);

		m_node->setMaterialTexture(0, driver->getTexture( settings.terrain_texture.c_str()));
		m_node->setMaterialTexture(1, driver->getTexture(settings.terrain_detail_texture.c_str()));

		m_node->setMaterialType(irr::video::EMT_DETAIL_MAP);

		m_node->scaleTexture(1.0f, 2.0f);
	});
}

Pakal::BasicTaskPtr Pakal::TerrainComponent_Irrlicht::destroy()
{
	return m_system->execute_block([=]()
	{
		m_node->remove();
		m_node = nullptr;
	});
}

void Pakal::TerrainComponent_Irrlicht::set_position(const tmath::vector3df& position)
{
	m_system->execute_block([=]()
	{
		m_node->setPosition({ position.x, position.y, position.z });
	});
}
