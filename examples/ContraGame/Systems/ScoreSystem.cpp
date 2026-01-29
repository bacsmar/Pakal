///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ScoreSystem.cpp
// Creation Date: January 2026
//
// Purpose: Score tracking and lives management implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ScoreSystem.h"
#include <algorithm>

using namespace Pakal;

const int ScoreSystem::SCORE_ENEMY_KILL;
const int ScoreSystem::SCORE_COLLECTIBLE;
const int ScoreSystem::SCORE_COMBO_MULTIPLIER;

ScoreSystem::ScoreSystem()
	: m_score(0)
	, m_highScore(0)
	, m_lives(3)
	, m_combo(0)
	, m_comboTimer(0.0f)
	, m_comboTimeLimit(3.0f)
{
}

void ScoreSystem::initialize()
{
	// Load high score from persistent storage
	// TODO: Implement save/load system
	// m_highScore = load_high_score();
	m_highScore = 0;

	reset();
}

void ScoreSystem::reset()
{
	m_score = 0;
	m_lives = 3;
	m_combo = 0;
	m_comboTimer = 0.0f;

	on_score_changed(m_score);
	on_lives_changed(m_lives);
	on_combo_changed(m_combo);
}

void ScoreSystem::add_score(int points)
{
	if (points <= 0)
		return;

	m_score += points;

	// Update high score
	update_high_score();

	on_score_changed(m_score);
}

void ScoreSystem::add_life()
{
	m_lives++;
	on_lives_changed(m_lives);
}

void ScoreSystem::lose_life()
{
	if (m_lives <= 0)
		return;

	m_lives--;
	on_lives_changed(m_lives);

	if (m_lives <= 0)
	{
		on_game_over();
	}
}

void ScoreSystem::add_kill()
{
	// Increment combo
	m_combo++;
	m_comboTimer = m_comboTimeLimit;

	// Calculate score with combo multiplier
	int baseScore = SCORE_ENEMY_KILL;
	int comboBonus = (m_combo - 1) * SCORE_COMBO_MULTIPLIER;
	int totalScore = baseScore + comboBonus;

	add_score(totalScore);
	on_combo_changed(m_combo);
}

void ScoreSystem::reset_combo()
{
	if (m_combo > 0)
	{
		m_combo = 0;
		m_comboTimer = 0.0f;
		on_combo_changed(m_combo);
	}
}

void ScoreSystem::update(float deltaTime)
{
	// Update combo timer
	if (m_combo > 0 && m_comboTimer > 0.0f)
	{
		m_comboTimer -= deltaTime;

		if (m_comboTimer <= 0.0f)
		{
			reset_combo();
		}
	}
}

void ScoreSystem::update_high_score()
{
	if (m_score > m_highScore)
	{
		bool wasHighScore = (m_highScore > 0);
		m_highScore = m_score;

		if (wasHighScore)
		{
			on_high_score_beaten();
		}

		// TODO: Save high score to persistent storage
		// save_high_score(m_highScore);
	}
}
