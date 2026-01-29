///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ScoreSystem.h
// Creation Date: January 2026
//
// Purpose: Score tracking and lives management
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Config.h"
#include "Event.h"

namespace Pakal
{
	class _PAKALExport ScoreSystem
	{
	public:
		ScoreSystem();
		~ScoreSystem() = default;

		void initialize();
		void reset();

		// Score management
		void add_score(int points);
		int get_score() const { return m_score; }
		int get_high_score() const { return m_highScore; }

		// Lives management
		void add_life();
		void lose_life();
		void set_lives(int lives) { m_lives = lives; }
		int get_lives() const { return m_lives; }

		// Game state
		bool is_game_over() const { return m_lives <= 0; }

		// Combo system
		void add_kill();
		void reset_combo();
		int get_combo() const { return m_combo; }
		float get_combo_timer() const { return m_comboTimer; }

		void update(float deltaTime);

		// Events
		Event<int> on_score_changed;      // Passes new score
		Event<int> on_lives_changed;      // Passes new lives count
		Event<int> on_combo_changed;      // Passes new combo count
		Event<> on_game_over;
		Event<> on_high_score_beaten;

	private:
		void update_high_score();

		int m_score;
		int m_highScore;
		int m_lives;
		int m_combo;
		float m_comboTimer;
		float m_comboTimeLimit;

		// Score values
		static const int SCORE_ENEMY_KILL = 100;
		static const int SCORE_COLLECTIBLE = 50;
		static const int SCORE_COMBO_MULTIPLIER = 10;
	};
}
