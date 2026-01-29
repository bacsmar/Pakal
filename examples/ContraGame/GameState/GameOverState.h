///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: GameOverState.h
// Creation Date: January 2026
//
// Purpose: Game over screen state
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "BaseGameState.h"

namespace Pakal
{
	class _PAKALExport GameOverState : public BaseGameState
	{
	public:
		GameOverState();
		~GameOverState() override = default;

		void set_final_score(int score) { m_finalScore = score; }
		void set_high_score(int score) { m_highScore = score; }
		void set_is_victory(bool victory) { m_isVictory = victory; }

	protected:
		void on_initialize(Engine* engine) override;
		void on_terminate(Engine* engine) override;
		void on_update(unsigned long dtMilliseconds) override;
		void on_activate(Engine* engine) override;
		void on_deactivate(Engine* engine) override;

	private:
		void handle_input();
		void return_to_menu();
		void retry();

		Engine* m_engine;
		int m_finalScore;
		int m_highScore;
		bool m_isVictory;
		int m_selectedOption;
	};
}
