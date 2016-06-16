#include "TerrainComponent_Irrlicht.h"
#include <irrlicht/StreamIrrlitch.h>

Pakal::TerrainComponent_Irrlicht::TerrainComponent_Irrlicht(IrrGraphicsSystem* irrGraphicsSystem):
	m_system(irrGraphicsSystem), m_node(nullptr), m_terrain_selector(nullptr)
{
}

Pakal::TerrainComponent_Irrlicht::~TerrainComponent_Irrlicht()
{
	m_system = nullptr;
	ASSERT(m_node == nullptr);
}

void Pakal::TerrainComponent_Irrlicht::show()
{
	m_system->execute_block([=]()
	{
		m_node->setVisible(true);	
	});
}

void Pakal::TerrainComponent_Irrlicht::hide()
{
	m_system->execute_block([=]()
	{
		m_node->setVisible(false);
	});
}

Pakal::BasicTaskPtr Pakal::TerrainComponent_Irrlicht::initialize(const Settings& settings)
{
	return m_system->execute_block([=]()
		{
			ASSERT(m_node == nullptr);
			ASSERT_MSG(settings.highmap_file_name.size() > 0, "no terrain highmap texture");
			ASSERT(settings.terrain_texture.size() > 0);
			ASSERT(settings.terrain_detail_texture.size() > 0);
			auto driver = m_system->get_driver();
			auto smgr = m_system->get_smgr();
			//auto terrainFile = ResourceMgr.open_read_resource(settings.highmap_file_name);
			//m_node = m_system->get_smgr()->addTerrainSceneNode(new StreamIrrlitch(terrainFile));
			m_node = smgr->addTerrainSceneNode(settings.highmap_file_name.c_str());

			m_node->setMaterialFlag(irr::video::EMF_LIGHTING, false);
			//m_node->setMaterialFlag(irr::video::EMF_WIREFRAME, true);

			m_node->setMaterialTexture(0, driver->getTexture(settings.terrain_texture.c_str()));
			m_node->setMaterialTexture(1, driver->getTexture(settings.terrain_detail_texture.c_str()));

			m_node->setMaterialType(irr::video::EMT_DETAIL_MAP);

			//m_terrain_selector = smgr->createTerrainTriangleSelector(m_node);

			//auto camera = smgr->getActiveCamera();
			//// create collision response animator and attach it to the camera
			//irr::scene::ISceneNodeAnimator* anim = smgr->createCollisionResponseAnimator(
			//	m_terrain_selector, camera, irr::core::vector3df(60, 100, 60),
			//	irr::core::vector3df(0, 0, 0),
			//	irr::core::vector3df(0, 50, 0));
			//m_terrain_selector->drop();
			//camera->addAnimator(anim);
			//anim->drop();

			//m_node->setScale({ 0.5f, 0.5f, 0.5f });
			//m_node->setScale({ 10.5f, 10.5f, 10.5f });
			//m_node->scaleTexture(1.0f, 1.0f);
		});
}

Pakal::BasicTaskPtr Pakal::TerrainComponent_Irrlicht::destroy()
{
	return m_system->execute_block([=]()
	{
		if (m_node)
		{
			m_node->remove();
			m_node = nullptr;
		}
	});
}

void Pakal::TerrainComponent_Irrlicht::set_position(const tmath::vector3df& position)
{
	m_system->execute_block([=]()
		{
			m_node->setPosition({position.x, position.y, position.z});
		});
}
