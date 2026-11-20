#pragma once
#include "AsyncComponent.h"
#include "EntityHandle.h"
#include "PakalMath.h"

namespace Pakal
{
	class AsyncEntity;

	class _PAKALExport CameraComponent2D : public AsyncComponent
	{
		DECLARE_RTTI_WITH_BASE(CameraComponent2D, AsyncComponent);

	public:
		CameraComponent2D() = default;
		virtual ~CameraComponent2D() = default;

		virtual void set_orthographic(float width, float height, float nearPlane = 0.1f, float farPlane = 100.0f) = 0;
		// Where on the screen the camera draws, in pixels of the back buffer. A width or a height of 0 is automatic: the whole
		// screen in the shape of the camera's view (the bars of what is left over, at the sides or above and below)
		virtual void set_viewport(int x, int y, int width, int height) = 0;
		virtual void set_position(float x, float y) = 0;
		virtual void set_zoom(float zoom) = 0;
		virtual void follow_target(EntityHandle target, float smoothness = 0.1f) = 0;
		virtual void follow_target(AsyncEntity* target, float smoothness = 0.1f) = 0;
		virtual void set_bounds(float minX, float minY, float maxX, float maxY) = 0;
		virtual void clear_bounds() = 0;
		virtual void update(float deltaTime) = 0;
		virtual tmath::vectorn<float, 2> get_position() const = 0;
	};
}
