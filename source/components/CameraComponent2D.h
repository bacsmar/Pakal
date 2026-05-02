#pragma once
#include "Component.h"
#include "EntityHandle.h"
#include "PakalMath.h"

namespace Pakal
{
	class Entity;

	class _PAKALExport CameraComponent2D : public Component
	{
		DECLARE_RTTI_WITH_BASE(CameraComponent2D, Component);

	public:
		CameraComponent2D() = default;
		virtual ~CameraComponent2D() = default;

		virtual void set_orthographic(float width, float height, float nearPlane = 0.1f, float farPlane = 100.0f) = 0;
		virtual void set_viewport(int x, int y, int width, int height) = 0;
		virtual void set_position(float x, float y) = 0;
		virtual void set_zoom(float zoom) = 0;
		virtual void follow_target(EntityHandle target, float smoothness = 0.1f) = 0;
		virtual void follow_target(Entity* target, float smoothness = 0.1f) = 0;
		virtual void set_bounds(float minX, float minY, float maxX, float maxY) = 0;
		virtual void clear_bounds() = 0;
		virtual void update(float deltaTime) = 0;
		virtual tmath::vectorn<float, 2> get_position() const = 0;
	};
}
