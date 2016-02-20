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
		void	set_target(const tmath::vector3df& targetPosition) override;
		void	set_position(tmath::vector3df position) override;
		tmath::vector3df get_position() const override;

		~CameraComponent_Irrlicht() override;	
	protected:
		IrrGraphicsSystem*				m_system;
		irr::scene::ICameraSceneNode*	m_camera;
	};
}
