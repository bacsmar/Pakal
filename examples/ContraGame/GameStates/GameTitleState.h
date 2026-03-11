///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: GameTitleState.h
// Purpose: Title screen state for Contra game
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "BaseGameState.h"

namespace Pakal
{
	class Engine;
	class GenericEntity;

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
		void create_prompt();
		void setup_camera();
		bool is_start_pressed() const;

		Engine* m_engine;
		GenericEntity* m_background;
		GenericEntity* m_prompt;
		GenericEntity* m_camera;
		float m_blinkTimer;
		bool m_promptVisible;
	};
}
