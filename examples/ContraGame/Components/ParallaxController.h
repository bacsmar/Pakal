///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ParallaxController.h
// Purpose: Component that produces parallax scrolling by offsetting a sprite relative to camera movement.
//
// Math:
//   sprite.x = anchor.x + camera.x * (1 - scroll_factor_x)
//   sprite.y = anchor.y + camera.y * (1 - scroll_factor_y)
//
// scroll_factor = 0   -> sprite fixed to screen (sky / infinite distance)
// scroll_factor = 0.3 -> slow-moving far background
// scroll_factor = 0.6 -> mid-distance background
// scroll_factor = 1.0 -> normal world object (no parallax effect)
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Component.h"
#include "math/vector2.h"

namespace Pakal
{
	class SpriteComponent2D;
	class CameraComponent2D;

	class ParallaxController : public Component
	{
		DECLARE_RTTI_WITH_BASE(ParallaxController, Component);

	public:
		ParallaxController() = default;
		virtual ~ParallaxController() = default;

		// Called once after creation to cache sibling components and record the sprite anchor position.
		void initialize();

		// Called every frame from the update loop.
		void update(float deltaTime);

		// Inject the active camera after the scene is loaded.
		void set_camera(CameraComponent2D* camera);

		// Scroll factors in [0,1].  Defaults: x=0.3, y=0 (horizontal parallax only).
		void set_scroll_factors(float sx, float sy);

		float get_scroll_factor_x() const { return m_scroll_factor_x; }
		float get_scroll_factor_y() const { return m_scroll_factor_y; }

	private:
		float m_scroll_factor_x = 0.3f;
		float m_scroll_factor_y = 0.0f;

		tmath::vectorn<float, 2> m_anchor = {0.f, 0.f};

		SpriteComponent2D* m_sprite  = nullptr;
		CameraComponent2D* m_camera  = nullptr;
		bool m_initialized           = false;
	};
}
