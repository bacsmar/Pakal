///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ContraGame.h
// Purpose: Main application class for Contra game
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "IPakalApplication.h"

namespace Pakal
{
	class Engine;
	
	class ContraGame : public IPakalApplication
	{
	public:
		const char* get_name() override { return "Contra Game Example"; }
		
		void start(Engine& engine) override;
		void end(Engine& engine) override;
		
	private:
		void register_components(Engine& engine);
		void setup_game_states(Engine& engine);
	};
}
