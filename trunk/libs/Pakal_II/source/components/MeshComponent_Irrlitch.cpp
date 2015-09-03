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
		if (!settings.mesh_name.empty())   set_mesh(settings.mesh_name);
		if (!settings.texture_name.empty())   set_texture(settings.texture_name);
		if (m_node)
		{ 
			set_position(settings.position);
			if (settings.size != tmath::vector3df())
			{
				set_size(settings.size);
			}
		}
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

tmath::vector3df MeshComponent_Irrlitch::get_size()
{
	auto v =  m_node->getTransformedBoundingBox().getExtent();
	v *= m_node->getScale();

	return tmath::vector3df(v.X,v.Y,v.Z);
}

void MeshComponent_Irrlitch::set_size(const tmath::vector3df& size)
{
	auto v =  m_node->getTransformedBoundingBox().getExtent();

	v = irr::core::vector3df(1,1,1) / v;

	v.X *= size.x;
	v.Y *= size.y;
	v.Z *= size.z;
	
	m_node->setScale(v);
}