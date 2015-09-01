#include "MeshComponent_Irrlitch.h"

#include "irrlicht/IrrGraphicsSystem.h"
#include "EventScheduler.h"

using namespace Pakal;


IrrGraphicsSystem* MeshComponent_Irrlitch::get_system()
{
	return static_cast<IrrGraphicsSystem*>(m_system);
}

MeshComponent_Irrlitch::MeshComponent_Irrlitch(IrrGraphicsSystem* irrGraphicsSystem) : MeshComponent(irrGraphicsSystem), m_node(nullptr) { }

void MeshComponent_Irrlitch::on_initialize() {}

void MeshComponent_Irrlitch::on_destroy()
{
	if (m_node)
	{
		m_node->remove();
	}
}

BasicTaskPtr MeshComponent_Irrlitch::LoadMeshAsync(const std::string& meshName)
{
	return EventScheduler::instance().execute_in_thread([=]()
	{
		m_mesh = get_system()->get_smgr()->getMesh(meshName.c_str());
		if (m_node)
		{
			m_node->remove();
		}
		m_node = get_system()->get_smgr()->addMeshSceneNode(m_mesh);
	},m_system->get_thread_id());
}

BasicTaskPtr MeshComponent_Irrlitch::LoadTextureAsync(const std::string& textureName)
{
	return EventScheduler::instance().execute_in_thread([=]()
	{
		m_texture = get_system()->get_driver()->getTexture(textureName.c_str());

		m_node->setMaterialFlag(irr::video::EMF_LIGHTING,false);
		m_node->setMaterialTexture(0,m_texture);
		m_node->setVisible(true);
	},m_system->get_thread_id());
}

void MeshComponent_Irrlitch::setPosition(const tmath::vector3df& position)
{
	irr::core::vector3df v(position.x,position.y,position.z);
	m_node->setPosition(v);
}

tmath::vector3df MeshComponent_Irrlitch::getPosition()
{
	auto vector3D = m_node->getPosition();

	return tmath::vector3df(vector3D.X,vector3D.Y,vector3D.Z);
}