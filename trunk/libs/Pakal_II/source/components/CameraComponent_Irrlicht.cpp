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

Pakal::BasicTaskPtr Pakal::CameraComponent_Irrlicht::set_target(const tmath::vector3df& targetPosition)
{
	return m_system->execute_block([=]()
	{
		m_camera->setTarget({ targetPosition.x, targetPosition.y, targetPosition.z });
	});
}

Pakal::BasicTaskPtr Pakal::CameraComponent_Irrlicht::set_position(const tmath::vector3df& position)
{
	return m_system->execute_block([=]()
	{
		m_camera->setPosition({ position.x, position.y, position.z });
	});
}

void Pakal::CameraComponent_Irrlicht::set_target_and_position(const tmath::vector3df& targetPosition, const tmath::vector3df& position)
{
	m_system->execute_block([=]()
	{
		m_camera->setTarget({ targetPosition.x, targetPosition.y, targetPosition.z });
		m_camera->setPosition({ position.x, position.y, position.z });

	});
}

Pakal::tmath::vector3df Pakal::CameraComponent_Irrlicht::translate_cordinate_to_point(const Pakal::tmath::vector2df& cordinate, const Pakal::tmath::vector2df& scene_size)
{
	const irr::scene::SViewFrustum* f = m_camera->getViewFrustum();
	irr::core::vector3df nearLeftUp = f->getNearLeftUp();//--- get how many units we see
	//irr::core::vector3df lefttoright = f->getNearRightDown() - nearLeftUp;
	irr::core::vector3df lefttoright = f->getNearRightUp() - nearLeftUp;
	//irr::core::vector3df uptodown = f->getNearLeftUp() - nearLeftUp;
	irr::core::vector3df uptodown = f->getNearLeftDown() - nearLeftUp;
	Pakal::tmath::vector2df points_2d;
	points_2d.x = cordinate.x / scene_size.x;
	points_2d.y = cordinate.y / scene_size.y;

	auto irr_world_point = f->cameraPosition + (lefttoright * (points_2d.x - 0.5f)) + (uptodown * (points_2d.y - 0.5f));
	Pakal::tmath::vector3df pakal_world_point;
	pakal_world_point.x = irr_world_point.X;
	pakal_world_point.y = irr_world_point.Y;
	pakal_world_point.z = irr_world_point.Z;


	return pakal_world_point;
}

Pakal::tmath::vector3df Pakal::CameraComponent_Irrlicht::cast_ray_from_screen_point(const Pakal::tmath::vector2df & cordinate, const Pakal::tmath::vector2df & screen_size)
{
  
	const irr::scene::SViewFrustum* f = m_camera->getViewFrustum();
	irr::core::vector3df farLeftUp = f->getFarLeftUp();
	irr::core::vector3df lefttoright = f->getFarRightUp() - farLeftUp;
	irr::core::vector3df uptodown = f->getFarLeftDown() - farLeftUp;


	Pakal::tmath::vector2df  proportion = { cordinate.x / screen_size.x, cordinate.y / screen_size.y };
	auto irr_vector3df = farLeftUp + (lefttoright * proportion.x) + (uptodown * proportion.y);
	return { irr_vector3df.X,irr_vector3df.Y,irr_vector3df.Z};
}

Pakal::tmath::vector3df Pakal::CameraComponent_Irrlicht::get_position() const
{
	const auto& position = m_camera->getPosition();
	return{ position.X , position.Y, position.Z };
}

void Pakal::CameraComponent_Irrlicht::bind_target_and_rotation(bool bound)
{	
	m_camera->bindTargetAndRotation(bound);
}

Pakal::CameraComponent_Irrlicht::~CameraComponent_Irrlicht()
{
	//ASSERT(m_camera == nullptr);
}
