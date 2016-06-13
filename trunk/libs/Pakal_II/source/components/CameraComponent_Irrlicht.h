#pragma once
#include "CameraComponent.h"
#include <ICameraSceneNode.h>

namespace Pakal
{
	class IrrGraphicsSystem;

	class _PAKALExport CameraComponent_Irrlicht : public CameraComponent
	{	
		DECLARE_RTTI_WITH_BASE(CameraComponent_Irrlicht, CameraComponent);	
	public:		

		explicit CameraComponent_Irrlicht(IrrGraphicsSystem* system);

		BasicTaskPtr initialize(const Settings& settings) override;
		BasicTaskPtr terminate() override;

		void	set_rotation(const tmath::vector3df & rotation) override;
		tmath::vector3df	get_rotation() const override;
		BasicTaskPtr	set_target(const tmath::vector3df & targetPosition) override;
		BasicTaskPtr	set_position(const tmath::vector3df & position) override;
		tmath::vector3df get_position() const override;
		
		// TODO: better implementation
		void	set_target_and_position(const tmath::vector3df & target, const tmath::vector3df & position) override;
		tmath::vector3df translate_cordinate_to_point(const Pakal::tmath::vector2df & cordinate, const Pakal::tmath::vector2df & screen_size)override;
		tmath::vector3df cast_ray_from_screen_point(const Pakal::tmath::vector2df & cordinate, const Pakal::tmath::vector2df & screen_size)override;		

		void bind_target_and_rotation(bool bound) override;

		~CameraComponent_Irrlicht() override;	
	protected:
		IrrGraphicsSystem*				m_system;
		irr::scene::ICameraSceneNode*	m_camera;
	};
}
