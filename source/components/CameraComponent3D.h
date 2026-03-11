#pragma once
#include "Component.h"

namespace Pakal
{

	class _PAKALExport CameraComponent3D : public Component
	{
		DECLARE_RTTI_WITH_BASE(CameraComponent3D, Component);

	public:
		CameraComponent3D() = default;
		virtual ~CameraComponent3D() = default;

		virtual void set_perspective(float fovYDegrees, float nearPlane = 0.1f, float farPlane = 2000.0f) = 0;
		virtual void set_viewport(int x, int y, int width, int height) = 0;
		virtual void set_position(float x, float y, float z) = 0;
		virtual void set_rotation(float yawRadians, float pitchRadians) = 0;
		virtual void look_at(float x, float y, float z) = 0;
		virtual void clear_look_at() = 0;
		virtual void update(float deltaTime) = 0;
	};
}
