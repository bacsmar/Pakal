///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: MenuState.cpp
// Creation Date: January 2026
//
// Purpose: Main menu state implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "MenuState.h"
#include "Engine.h"
#include "GameStateManager.h"
#include "IInputManager.h"

using namespace Pakal;

MenuState::MenuState()
	: BaseGameState("Menu")
	, m_engine(nullptr)
	, m_selectedOption(0)
{
}

void MenuState::on_initialize(Engine* engine)
{
	m_engine = engine;

	// TODO: Setup UI
	// auto uiManager = engine->get_ui_manager();
	// if (uiManager)
	// {
	//     // Create title text
	//     // Create menu options: "Start Game", "Quit"
	//     // Setup selection highlight
	// }
}

void MenuState::on_terminate(Engine* engine)
{
	// TODO: Cleanup UI
	m_engine = nullptr;
}

void MenuState::on_update(unsigned long dtMilliseconds)
{
	handle_input();

	// TODO: Update UI animations
}

void MenuState::on_activate(Engine* engine)
{
	// Menu is now active
}

void MenuState::on_deactivate(Engine* engine)
{
	// Menu is being hidden
}

void MenuState::handle_input()
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
	//         start_game();
	//     else if (m_selectedOption == 1)
	//         quit_game();
	// }
}

void MenuState::start_game()
{
	// Transition to gameplay state
	if (m_engine)
	{
		auto gameStateManager = m_engine->game_state_manager();
		if (gameStateManager)
		{
			// TODO: Push gameplay state
			// gameStateManager->push_state("GamePlay");
		}
	}
}

void MenuState::quit_game()
{
	// Exit application
	if (m_engine)
	{
		// TODO: Implement proper shutdown
		// m_engine->shutdown();
	}
}
