///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: GameTitleState.cpp
// Purpose: Title screen state for Contra game - Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "GameTitleState.h"
#include "GamePlayState.h"
#include "Engine.h"
#include "GameStateManager.h"
#include "EntityManager.h"
#include "GenericEntity.h"
#include "LogMgr.h"
#include "InputManager_Polling.h"
#include "EventArgs.h"

#include "components/SpriteComponent2D.h"
#include "components/CameraComponent2D.h"

namespace Pakal
{
	GameTitleState::GameTitleState() : BaseGameState("Title"),
		m_engine(nullptr),
		m_background(nullptr),
		m_overlay(nullptr),
		m_prompt(nullptr),
		m_camera(nullptr),
		m_blinkTimer(0.0f),
		m_promptVisible(true)
	{
	}

	GameTitleState::~GameTitleState()
	{
	}

	void GameTitleState::on_initialize(Engine* engine)
	{
		LOG_INFO("[GameTitleState] Initializing title screen");
		m_engine = engine;
		m_blinkTimer = 0.0f;
		m_promptVisible = true;

		setup_camera();
		create_background();
		create_overlay();
		create_prompt();

		LOG_INFO("[GameTitleState] Press any key to start");
	}

	void GameTitleState::on_terminate(Engine* engine)
	{
		LOG_INFO("[GameTitleState] Terminating title screen");
		m_background = nullptr;
		m_overlay = nullptr;
		m_prompt = nullptr;
		m_camera = nullptr;
	}

	void GameTitleState::on_activate(Engine* engine)
	{
		LOG_INFO("[GameTitleState] Activating title screen");
		m_blinkTimer = 0.0f;
		m_promptVisible = true;
	}

	void GameTitleState::on_deactivate(Engine* engine)
	{
		LOG_INFO("[GameTitleState] Deactivating title screen");
	}

	void GameTitleState::on_update(unsigned long dtMilliseconds)
	{
		const float deltaTime = dtMilliseconds / 1000.0f;
		m_blinkTimer += deltaTime;

		if (m_blinkTimer >= 0.5f)
		{
			m_blinkTimer = 0.0f;
			m_promptVisible = !m_promptVisible;

			if (m_prompt)
			{
				auto* sprite = m_prompt->get_component<SpriteComponent2D>();
				if (sprite)
				{
					const float alpha = m_promptVisible ? 1.0f : 0.2f;
					sprite->set_color(0.9f, 0.9f, 0.2f, alpha);
				}
			}
		}

		if (is_start_pressed())
		{
			LOG_INFO("[GameTitleState] Start pressed - switching to gameplay");
			auto* manager = get_manager();
			if (manager)
			{
				manager->transition_to_state(new GamePlayState(), true);
			}
		}
	}

	void GameTitleState::setup_camera()
	{
		LOG_INFO("[GameTitleState] Setting up camera...");
		auto* entityMgr = m_engine->entity_manager();
		m_camera = dynamic_cast<GenericEntity*>(entityMgr->create_entity("Pakal::GenericEntity", "title_camera"));
		if (!m_camera)
		{
			LOG_ERROR("[GameTitleState] Failed to create camera entity!");
			return;
		}

		LOG_INFO("[GameTitleState] Creating CameraComponent2D...");
		auto* camera = m_camera->create_component<CameraComponent2D>();
		if (camera)
		{
			LOG_INFO("[GameTitleState] Camera component created successfully");
			camera->set_orthographic(32.0f, 18.0f);
			camera->set_viewport(0, 0, 1280, 720);
			camera->set_position(0.0f, 0.0f);
			camera->set_zoom(1.0f);
		}
		else
		{
			LOG_ERROR("[GameTitleState] Failed to create camera component!");
		}
	}

	void GameTitleState::create_background()
	{
		LOG_INFO("[GameTitleState] Creating background...");
		auto* entityMgr = m_engine->entity_manager();
		m_background = dynamic_cast<GenericEntity*>(entityMgr->create_entity("Pakal::GenericEntity", "title_background"));
		if (!m_background)
		{
			LOG_ERROR("[GameTitleState] Failed to create background entity!");
			return;
		}

		LOG_INFO("[GameTitleState] Creating background SpriteComponent2D...");
		auto* sprite = m_background->create_component<SpriteComponent2D>();
		if (sprite)
		{
			LOG_INFO("[GameTitleState] Background sprite component created successfully");
		sprite->set_texture("Assets/sprites/background.png");
		sprite->set_position(0.0f, 0.0f);
		sprite->set_color(1.0f, 1.0f, 1.0f, 1.0f);
		sprite->set_scale(32.0f, 18.0f);
			sprite->set_layer(0);
		}
		else
		{
			LOG_ERROR("[GameTitleState] Failed to create background sprite component!");
		}
	}

	void GameTitleState::create_overlay()
	{
		LOG_INFO("[GameTitleState] Creating dark overlay...");
		auto* entityMgr = m_engine->entity_manager();
		m_overlay = dynamic_cast<GenericEntity*>(entityMgr->create_entity("Pakal::GenericEntity", "title_overlay"));
		if (!m_overlay)
		{
			LOG_ERROR("[GameTitleState] Failed to create overlay entity!");
			return;
		}

		auto* sprite = m_overlay->create_component<SpriteComponent2D>();
		if (sprite)
		{
			sprite->create_solid_color(0xFFFFFFFF, 1, 1);
			sprite->set_position(0.0f, 0.0f);
			sprite->set_scale(32.0f, 18.0f);
			sprite->set_color(0.05f, 0.08f, 0.12f, 0.55f);
			sprite->set_layer(2);
		}
	}

	void GameTitleState::create_prompt()
	{
		LOG_INFO("[GameTitleState] Creating prompt...");
		auto* entityMgr = m_engine->entity_manager();
		m_prompt = dynamic_cast<GenericEntity*>(entityMgr->create_entity("Pakal::GenericEntity", "title_prompt"));
		if (!m_prompt)
		{
			LOG_ERROR("[GameTitleState] Failed to create prompt entity!");
			return;
		}

		LOG_INFO("[GameTitleState] Creating prompt SpriteComponent2D...");
		auto* sprite = m_prompt->create_component<SpriteComponent2D>();
		if (sprite)
		{
			LOG_INFO("[GameTitleState] Prompt sprite component created successfully");
		sprite->create_solid_color(0xFFFFFFFF, 1, 1);
		sprite->set_position(0.0f, -6.0f);
		sprite->set_color(0.95f, 0.85f, 0.20f, 1.0f);
		sprite->set_scale(10.0f, 0.8f);
			sprite->set_layer(1);
		}
		else
		{
			LOG_ERROR("[GameTitleState] Failed to create prompt sprite component!");
		}
	}

	bool GameTitleState::is_start_pressed() const
	{
		auto& input = InputManager_Polling::instance();
		return input.poll_key_down(Key::Space) ||
			input.poll_key_down(Key::Return) ||
			input.poll_key_down(Key::Z) ||
			input.poll_key_down(Key::X) ||
			input.poll_key_down(Key::Up) ||
			input.poll_key_down(Key::Down) ||
			input.poll_key_down(Key::Left) ||
			input.poll_key_down(Key::Right);
	}
}
