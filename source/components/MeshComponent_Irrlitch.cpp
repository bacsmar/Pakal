#include "MeshComponent_Irrlitch.h"
#include "irrlicht/IrrGraphicsSystem.h"

using namespace Pakal;

MeshComponent_Irrlitch::~MeshComponent_Irrlitch() 
{
	m_system = nullptr;
	ASSERT( m_node == nullptr);
}

BasicTaskPtr MeshComponent_Irrlitch::initialize(const Settings& settings)
{
	return m_system->execute_block([=]()
	{
		if (!settings.MeshName.empty())   set_mesh(settings.MeshName);
		if (!settings.TextureName.empty())   set_texture(settings.TextureName);
		set_position(settings.Position);		
	});
}

BasicTaskPtr MeshComponent_Irrlitch::destroy()
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

BasicTaskPtr MeshComponent_Irrlitch::set_mesh(const std::string& meshName)
{
	return m_system->execute_block([=]()
	{
		if (m_node)
			m_node->remove();

		m_mesh = m_system->get_smgr()->getMesh(meshName.c_str());

		m_node = m_system->get_smgr()->addMeshSceneNode(m_mesh);

		m_node->setMaterialFlag(irr::video::EMF_LIGHTING,false);
		m_node->setVisible(true);
	});
}

BasicTaskPtr MeshComponent_Irrlitch::set_texture(const std::string& textureName)
{
	return m_system->execute_block([=]()
	{
		m_texture = m_system->get_driver()->getTexture(textureName.c_str());
		m_node->setMaterialTexture(0,m_texture);
	});

}

void MeshComponent_Irrlitch::set_position(const tmath::vector3df& position)
{
	irr::core::vector3df v(position.x,position.y,position.z);
	m_node->setPosition(v);
}

tmath::vector3df MeshComponent_Irrlitch::get_position()
{
	auto vector3D = m_node->getPosition();

	return tmath::vector3df(vector3D.X,vector3D.Y,vector3D.Z);
}

