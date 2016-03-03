#include "CameraComponent_Irrlicht.h"
#include "Task.h"
#include "irrlicht/IrrGraphicsSystem.h"

Pakal::BasicTaskPtr Pakal::CameraComponent_Irrlicht::initialize(const Settings& settings)
{
	ASSERT(m_camera == nullptr);
	return m_system->execute_block([=]()
	{
		auto position = irr::core::vector3df(settings.position.x, settings.position.y, settings.position.z);
		auto look_at = irr::core::vector3df(settings.look_at.x, settings.look_at.y, settings.look_at.z);

		m_camera = m_system->get_smgr()->addCameraSceneNode(nullptr, position, look_at);

		//auto camera = smgr->addCameraSceneNode(nullptr, vector3df(0,0,-10), vector3df(0,0,0));
		//auto camera = smgr->addCameraSceneNode(nullptr, vector3df(3,15,-10), vector3df(0,0,0));

		//irr::core::matrix4 MyMatrix;
		//MyMatrix.buildProjectionMatrixOrthoRH(16.0f, 12.0f, 15.5f, -32.5f);
		//camera->setProjectionMatrix(MyMatrix);
	});		
}

Pakal::BasicTaskPtr Pakal::CameraComponent_Irrlicht::terminate()
{
	return m_system->execute_block([=]()
	{
		if( m_system->get_smgr()->getActiveCamera() == m_camera)
		{
			m_system->get_smgr()->setActiveCamera(nullptr);
		}
		m_camera->remove();		
		m_camera = nullptr;
	});
}

Pakal::CameraComponent_Irrlicht::CameraComponent_Irrlicht(IrrGraphicsSystem* system) : m_camera(nullptr)
{
	m_system = system;
}

void Pakal::CameraComponent_Irrlicht::set_rotation(const tmath::vector3df & rotation)
{
	m_system->execute_block([=]()
	{
		m_camera->setRotation({ rotation.x, rotation.y, rotation.z });
	});
}

Pakal::tmath::vector3df Pakal::CameraComponent_Irrlicht::get_rotation() const
{
	auto rotation = m_camera->getRotation();
	return{ rotation.X , rotation.Y, rotation.Z };
}

void Pakal::CameraComponent_Irrlicht::set_target(const tmath::vector3df& targetPosition)
{
	m_system->execute_block([=]()
	{
		m_camera->setTarget({ targetPosition.x, targetPosition.y, targetPosition.z });
	});
}

void Pakal::CameraComponent_Irrlicht::set_position(tmath::vector3df position)
{
	m_system->execute_block([=]()
	{
		m_camera->setPosition({ position.x, position.y, position.z });
	});
}

Pakal::tmath::vector3df Pakal::CameraComponent_Irrlicht::get_position() const
{
	auto position = m_camera->getPosition();
	return{ position.X , position.Y, position.Z };
}

Pakal::CameraComponent_Irrlicht::~CameraComponent_Irrlicht()
{
	//ASSERT(m_camera == nullptr);
}
