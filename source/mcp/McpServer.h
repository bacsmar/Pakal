///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: McpServer.h
// Purpose: A small Model Context Protocol server. It speaks JSON-RPC 2.0 over HTTP (POST /mcp) on
//          127.0.0.1, so an AI client can call the tools a host registers (status, screenshots, and
//          later level editing). The server runs on its own thread; a tool that needs the graphics
//          thread marshals the work itself (GraphicsSystem::execute_block).
//
// Security: loopback only (on a phone, reached from a computer through `adb forward`). When a token is set, every
// request must carry "Authorization: Bearer <token>". A request with an Origin header (a web page's) is refused: a
// browser may post to 127.0.0.1 without asking first. Off unless the host starts it (PAKAL_MCP_PORT: EngineTools), and
// never in a players' copy (start_engine_tools).
//
// A tool's lifetime: remove_tool returns only after the calls of that tool already running have ended, so a tool may
// capture what its owner frees right after removing it. Call it from a thread no tool waits for (not the graphics
// thread: a tool may wait for it); a tool that removes itself does not wait for itself.
// Windows sockets or POSIX ones (Android, Linux).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Platform.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace Pakal
{
	class _PAKALExport McpServer
	{
	public:
		using ToolCall = std::function<nlohmann::json(const nlohmann::json& arguments)>;

		struct Tool
		{
			std::string description;
			nlohmann::json inputSchema; // JSON Schema of the arguments
			ToolCall call;              // returns the result as JSON; throws std::runtime_error on failure
		};

		McpServer() = default;
		~McpServer();
		McpServer(const McpServer&) = delete;
		McpServer& operator=(const McpServer&) = delete;

		// Before or after start() (a game adds its own when it starts); a name given again replaces the tool
		void add_tool(const std::string& name, Tool tool);
		// Removes it, and waits for the calls of it already running to end (see the lifetime above)
		void remove_tool(const std::string& name);
		// The server the host started (null when none): where a game or a genre adds its tools
		static McpServer* active();

		// Binds 127.0.0.1:port and starts serving. Returns false with 'error' set when it cannot bind.
		bool start(unsigned short port, const std::string& token, std::string& error);
		void stop();
		bool is_running() const { return m_running.load(); }

		// Answers one JSON-RPC message. Returns the response body, or an empty string for notifications.
		// Public so the protocol can be tested without sockets.
		std::string handle_message(const std::string& body);

	private:
		void serve();
		void handle_connection(uintptr_t socket);

		// a tool and how many calls of it are running (kept alive by a running call when it is removed or replaced)
		struct ToolEntry
		{
			Tool tool;
			int running = 0;
		};
		std::map<std::string, std::shared_ptr<ToolEntry>> m_tools;
		std::recursive_mutex m_toolsMutex;
		std::condition_variable_any m_toolsIdle;   // a call ended
		std::string m_token;
		std::thread m_thread;
		std::atomic<bool> m_running{ false };
		uintptr_t m_listenSocket = ~static_cast<uintptr_t>(0);
	};
}
