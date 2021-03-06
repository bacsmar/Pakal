#include "AnimatedMeshComponent_Irrlitch.h"
#include "irrlicht/IrrGraphicsSystem.h"
#include <irrlicht/MaterialManager.h>

using namespace Pakal;

AnimatedMeshComponent_Irrlitch::~AnimatedMeshComponent_Irrlitch()
{
	m_system = nullptr;
	ASSERT( m_node == nullptr);
}

BasicTaskPtr AnimatedMeshComponent_Irrlitch::initialize(const Settings& settings)
{
	return m_system->execute_block([=]()
	{
		if (!settings.mesh_name.empty())
		{
			set_mesh(settings.mesh_name);
		}
		if (!settings.texture_name.empty())
		{
			set_texture(settings.texture_name);
		}
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

BasicTaskPtr AnimatedMeshComponent_Irrlitch::destroy()
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

BasicTaskPtr AnimatedMeshComponent_Irrlitch::set_mesh(const std::string& meshName)
{
	return m_system->execute_block([=]()
	{
		if (m_node)
			m_node->remove();

		m_mesh = m_system->get_smgr()->getMesh(meshName.c_str());		
		m_node = m_system->get_smgr()->addAnimatedMeshSceneNode(m_mesh);

		m_node->setMaterialFlag(irr::video::EMF_LIGHTING, false);

		//auto mat = m_node->getMaterial(0).MaterialType;

		//auto materialManager = m_system->get_material_manager();		
		//auto materialType = materialManager->get_material(MaterialManager::MaterialType::EMT_TRANSPARENT_REF);		
		//m_node->setMaterialType(materialType);

		m_node->setVisible(true);
	});
}

BasicTaskPtr AnimatedMeshComponent_Irrlitch::set_texture(const std::string& textureName)
{
	return m_system->execute_block([=]()
	{
		m_texture = m_system->get_driver()->getTexture(textureName.c_str());
		m_node->setMaterialTexture(0,m_texture);
	});

}

void AnimatedMeshComponent_Irrlitch::set_position(const tmath::vector3df& position)
{
	irr::core::vector3df v(position.x,position.y,position.z);
	m_node->setPosition(v);
}

tmath::vector3df AnimatedMeshComponent_Irrlitch::get_position()
{
	const auto& vector3D = m_node->getPosition();

	return tmath::vector3df(vector3D.X,vector3D.Y,vector3D.Z);
}

tmath::vector3df AnimatedMeshComponent_Irrlitch::get_size()
{
	auto v =  m_node->getTransformedBoundingBox().getExtent();
	v *= m_node->getScale();

	return tmath::vector3df(v.X,v.Y,v.Z);
}

void AnimatedMeshComponent_Irrlitch::set_size(const tmath::vector3df& size)
{
	auto v =  m_node->getTransformedBoundingBox().getExtent();
	
	irr::f64 length = v.X*v.X + v.Y*v.Y + v.Z*v.Z;
	if (length == 0)
	{
		return;
	}
	v = (irr::f32)irr::core::reciprocal_squareroot(length) * v;

	v.X *= size.x;
	v.Y *= size.y;
	v.Z *= size.z;
	
	m_node->setScale(v);
}

void AnimatedMeshComponent_Irrlitch::set_angle(const tmath::vector3df& angle)
{
	m_node->setRotation(irr::core::vector3df(angle.x,angle.y,angle.z));
}

tmath::vector3df AnimatedMeshComponent_Irrlitch::get_angle()
{
	const auto& r = m_node->getRotation();
	return tmath::vector3df(r.X,r.Y,r.Z);
}