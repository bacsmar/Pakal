///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: QuestsPanel.h
// Purpose: The quests' development panel (Dear ImGui; docs/QUESTS.md, the tools): every quest kept, a running
//          one's stage, aliases, vars and what its exits wait on, why the last start gave 0; a stuck quest
//          moved from here (to a stage of its record, its checkpoint again, abandoned, failed); the
//          validator's report on the records loaded; the last transitions. A game draws it inside its own
//          development window, on the thread that steps the quests or under that thread's lock (its moves are
//          queued commands, carried out at the next step).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "quests/QuestArbiter.h"
#include "quests/QuestSystem.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Pakal::Quests
{
	struct PanelState
	{
		std::vector<std::string> findings;   // the validator's last report (the records loaded)
		// what the game found when it loaded its records (parse errors, a text key missing, a chain broken): a record
		// refused there is not in the system, so the panel's own check never sees it. The game fills it at its load
		std::vector<std::string> loadFindings;
		bool validated = false;
		std::map<uint64_t, int> stageChoice; // per quest: the stage a move goes to (an index into its record's stages)
	};

	// 'actions': the moves and the validator offered (a development build; a release's panel only reads)
	void draw_quests_panel(QuestSystem& quests, const Arbiter& arbiter, PanelState& state, bool actions);
}
