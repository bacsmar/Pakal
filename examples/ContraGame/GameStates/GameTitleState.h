///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: GameTitleState.h
// Purpose: Title screen state for Contra game
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "BaseGameState.h"
#include "EntityHandle.h"

namespace Pakal
{
	class Engine;

	class GameTitleState : public BaseGameState
	{
	public:
		GameTitleState();
		virtual ~GameTitleState();

	protected:
		void on_initialize(Engine* engine) override;
		void on_terminate(Engine* engine) override;
		void on_update(unsigned long dtMilliseconds) override;
		void on_activate(Engine* engine) override;
		void on_deactivate(Engine* engine) override;

	private:
		void create_background();
		void create_overlay();
		void create_prompt();
		void setup_camera();
		bool is_start_pressed() const;

		Engine* m_engine;
		EntityHandle m_background;
		EntityHandle m_overlay;
		EntityHandle m_prompt;
		EntityHandle m_camera;
		float m_blinkTimer;
		bool m_promptVisible;
	};
}
