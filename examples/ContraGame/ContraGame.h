///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ContraGame.h
// Creation Date: January 2026
//
// Purpose: Contra-style run & gun game application
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "IPakalApplication.h"

namespace Pakal
{
	class _PAKALExport ContraGame : public IPakalApplication
	{
	public:
		ContraGame();
		~ContraGame() override = default;

		const char* get_name() override { return "Contra Game Example"; }

		void start(Engine& engine) override;
		void end(Engine& engine) override;

	private:
		void register_components(Engine& engine);
		void register_game_states(Engine& engine);
		void load_resources(Engine& engine);
	};
}
