///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ParallaxController.cpp
// Purpose: Parallax scrolling component implementation.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ParallaxController.h"
#include "GenericEntity.h"
#include "components/SpriteComponent2D.h"
#include "components/CameraComponent2D.h"
#include "LogMgr.h"

namespace Pakal
{
	void ParallaxController::initialize()
	{
		if (m_initialized)
			return;

		auto* parent = dynamic_cast<GenericEntity*>(get_parent_entity());
		if (!parent)
		{
			LOG_WARNING("[ParallaxController] No parent entity — parallax disabled");
			return;
		}

		m_sprite = parent->get_component<SpriteComponent2D>();
		if (!m_sprite)
		{
			LOG_WARNING("[ParallaxController] Parent has no SpriteComponent2D — parallax disabled");
			return;
		}

		// Cache the sprite's initial world position as the parallax anchor.
		m_anchor = m_sprite->get_position();
		m_initialized = true;
	}

	void ParallaxController::update(float /*deltaTime*/)
	{
		if (!m_initialized || !m_sprite || !m_camera)
			return;

		const auto camPos = m_camera->get_position();

		const float newX = m_anchor.x + camPos.x * (1.0f - m_scroll_factor_x);
		const float newY = m_anchor.y + camPos.y * (1.0f - m_scroll_factor_y);

		m_sprite->set_position(newX, newY);
	}

	void ParallaxController::set_camera(CameraComponent2D* camera)
	{
		m_camera = camera;
	}

	void ParallaxController::set_scroll_factors(float sx, float sy)
	{
		m_scroll_factor_x = sx;
		m_scroll_factor_y = sy;
	}
}
