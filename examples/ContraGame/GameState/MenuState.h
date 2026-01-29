///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: MenuState.h
// Creation Date: January 2026
//
// Purpose: Main menu state
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "BaseGameState.h"

namespace Pakal
{
	class _PAKALExport MenuState : public BaseGameState
	{
	public:
		MenuState();
		~MenuState() override = default;

	protected:
		void on_initialize(Engine* engine) override;
		void on_terminate(Engine* engine) override;
		void on_update(unsigned long dtMilliseconds) override;
		void on_activate(Engine* engine) override;
		void on_deactivate(Engine* engine) override;

	private:
		void handle_input();
		void start_game();
		void quit_game();

		Engine* m_engine;
		int m_selectedOption;
	};
}
