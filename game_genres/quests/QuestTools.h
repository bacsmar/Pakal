///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: QuestTools.h
// Purpose: The quests' tools for a tester or an AI client over MCP (docs/QUESTS.md, the debug view): read-only in
//          this version. quests.state: the arbiter's claims, the tracked owner, the presentation and the game's
//          own part ("game", Arbiter::publish_game); quests.log: the decisions after a sequence number.
//          Added only where the QA tools are (a development build's server with a token, never a release:
//          qa_tools_allowed). A tool keeps the arbiter's published block alive while it reads, so a call in
//          flight while a level unloads reads that copy, never freed memory.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "quests/QuestArbiter.h"

namespace Pakal
{
	class McpServer;
}

namespace Pakal::Quests
{
	// On the server the host started (none: nothing), when the QA tools are allowed there (PAKAL_MCP_TOKEN set,
	// PAKAL_RELEASE not 1). Returns whether they were added
	bool add_tools(const Arbiter& arbiter);
	// The same on a given server, with the environment given (for tests)
	bool add_tools(McpServer& server, const Arbiter& arbiter, const char* token, const char* release);
	void remove_tools();
	void remove_tools(McpServer& server);
}
