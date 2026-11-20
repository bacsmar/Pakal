///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: QaTools.h
// Purpose: The QA hooks on the engine's MCP server (docs/QA_HOOKS.md): what a program needs
//          to drive any game the way a person would and to check where it is, without adb taps or SendInput.
//          - input.key, input.pad, input.touch, input.pointer: synthetic input, delivered by the platform
//            where its own input arrives and through the same path (IInputManager::queue_synthetic), so the
//            game and its interface cannot tell it from a person's;
//          - state: the game state stack (top first, each one's status line), the game's line, the UI
//            documents loaded (id, file, title, shown) and the time scale;
//          - log.tail: the last lines of the log (kept in memory from the server's start), filtered;
//          - time.scale: how fast the simulation runs (0 stops it; TimeScale.h says what follows it);
//          - quit: the game ends as when the person closes it.
//          docs/QA_HOOKS.md; tools/pakal_device.py drives them (key, tap, touch, pad, state, log, timescale,
//          quit, story).
//
// Security: they are added only where the engine's tools are (PAKAL_MCP_PORT; on Android only a --tools build has
// source/mcp at all) and only when the server requires a token (PAKAL_MCP_TOKEN) and the build is not a release
// (PAKAL_RELEASE=1): start_engine_tools decides (EngineTools.cpp).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "mcp/McpServer.h"

#include <string>
#include <vector>

namespace Pakal
{
	class Engine;

	// Adds the QA tools to a server, and starts keeping the log's last lines in memory
	_PAKALExport void add_qa_tools(McpServer& server, Engine& engine);

	// Whether they may be added: the server requires a token (PAKAL_MCP_TOKEN, not empty) and the build is not a release
	// (PAKAL_RELEASE is not "1"). Otherwise false, with the reason in 'why'.
	_PAKALExport bool qa_tools_allowed(const char* token, const char* release, std::string& why);

	// The log's last lines kept in memory (log.tail), oldest first: those after 'after' (a line number; 0: all kept), at
	// most 'count', that contain 'filter' (any case; empty: all) and are at least 'minLevel' (LogMgr::LogLevel). 'last'
	// gets the number of the newest line kept.
	_PAKALExport std::vector<std::string> qa_log_tail(size_t count, const std::string& filter, int minLevel,
		unsigned long long after, unsigned long long& last);
	// Starts keeping them (add_qa_tools does)
	_PAKALExport void qa_log_keep();
}
