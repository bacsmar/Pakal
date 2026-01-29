///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: GameOverState.cpp
// Creation Date: January 2026
//
// Purpose: Game over screen implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "GameOverState.h"
#include "Engine.h"
#include "GameStateManager.h"
#include "IInputManager.h"

using namespace Pakal;

GameOverState::GameOverState()
	: BaseGameState("GameOver")
	, m_engine(nullptr)
	, m_finalScore(0)
	, m_highScore(0)
	, m_isVictory(false)
	, m_selectedOption(0)
{
}

void GameOverState::on_initialize(Engine* engine)
{
	m_engine = engine;

	// TODO: Setup UI
	// auto uiManager = engine->get_ui_manager();
	// if (uiManager)
	// {
	//     if (m_isVictory)
	//     {
	//         // Display "VICTORY!" text
	//     }
	//     else
	//     {
	//         // Display "GAME OVER" text
	//     }
	//     
	//     // Display final score
	//     // Display high score
	//     // Display options: "Retry", "Main Menu"
	// }
}

void GameOverState::on_terminate(Engine* engine)
{
	// TODO: Cleanup UI
	m_engine = nullptr;
}

void GameOverState::on_update(unsigned long dtMilliseconds)
{
	handle_input();

	// TODO: Update UI animations
}

void GameOverState::on_activate(Engine* engine)
{
	// Game over screen is now active
}

void GameOverState::on_deactivate(Engine* engine)
{
	// Game over screen is being hidden
}

void GameOverState::handle_input()
{
	if (!m_engine)
		return;

	auto inputManager = m_engine->input_manager();
	if (!inputManager)
		return;

	// TODO: Handle menu navigation
	// Up/Down to select option
	// Enter/Space to confirm

	// Stub implementation:
	// if (inputManager->is_key_pressed(Key::Up))
	// {
	//     m_selectedOption = (m_selectedOption - 1 + 2) % 2;
	// }
	// else if (inputManager->is_key_pressed(Key::Down))
	// {
	//     m_selectedOption = (m_selectedOption + 1) % 2;
	// }
	// else if (inputManager->is_key_pressed(Key::Enter))
	// {
	//     if (m_selectedOption == 0)
	//         retry();
	//     else if (m_selectedOption == 1)
	//         return_to_menu();
	// }
}

void GameOverState::return_to_menu()
{
	// Return to main menu
	if (m_engine)
	{
		auto gameStateManager = m_engine->game_state_manager();
		if (gameStateManager)
		{
			// TODO: Pop to menu state
			// gameStateManager->pop_state();  // Pop game over
			// gameStateManager->pop_state();  // Pop gameplay
			// This should return to menu
		}
	}
}

void GameOverState::retry()
{
	// Restart game
	if (m_engine)
	{
		auto gameStateManager = m_engine->game_state_manager();
		if (gameStateManager)
		{
			// TODO: Pop game over, reset gameplay
			// gameStateManager->pop_state();  // Pop game over
			// Then the gameplay state should reset
		}
	}
}
