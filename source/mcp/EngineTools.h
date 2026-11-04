///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: EngineTools.h
// Purpose: The engine's tools on its MCP server, the same on every platform (PakalPlayer on a desktop,
//          the Android app): what a running game costs and how it is drawn, read and changed while it
//          plays, by a program on the same machine or on a computer through `adb forward` (tools/
//          pakal_device.py, the level editor's Device panel, an agent). docs/PROFILING.md lists them.
//          - get_status, get_stats (the last second: frame times, sections, counters: GPU per pass, draws,
//            triangles, memory, temperature, the governor), get_profile (the records after a clock),
//            record_start / record_stop (a recording in the device's own files);
//          - get_settings / set_settings: the quality level and its presets' fields, the post-process and the
//            shadows, the render scale, FSR, the sharpness, the governor;
//          - set_camera: the camera held at a pose, or let go; screenshot.
//          A game adds its own tools to the same server (McpServer::active()), e.g. its data's tuned values
//          written back ("write_back").
//
// Off unless asked for: PAKAL_MCP_PORT=<port> (and PAKAL_MCP_TOKEN=<token> for a bearer token) in the
// environment, or the Android app's pakal_env.txt. It listens on 127.0.0.1 only.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "mcp/McpServer.h"

#include <memory>

namespace Pakal
{
	class Engine;

	// Adds the engine's tools to a server (its graphics system's, when it has one)
	_PAKALExport void add_engine_tools(McpServer& server, Engine& engine);
	// The server PAKAL_MCP_PORT asks for, with the engine's tools, started; null when it is not asked for or cannot start
	_PAKALExport std::unique_ptr<McpServer> start_engine_tools(Engine& engine);
}
