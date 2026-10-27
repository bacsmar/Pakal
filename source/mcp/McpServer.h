///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: McpServer.h
// Purpose: A small Model Context Protocol server. It speaks JSON-RPC 2.0 over HTTP (POST /mcp) on
//          127.0.0.1, so an AI client can call the tools a host registers (status, screenshots, and
//          later level editing). The server runs on its own thread; a tool that needs the graphics
//          thread marshals the work itself (GraphicsSystem::execute_block).
//
// Security: loopback only. When a token is set, every request must carry "Authorization: Bearer <token>".
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Platform.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
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

		// Register before start(). Names must be unique.
		void add_tool(const std::string& name, Tool tool);

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

		std::map<std::string, Tool> m_tools;
		std::string m_token;
		std::thread m_thread;
		std::atomic<bool> m_running{ false };
		uintptr_t m_listenSocket = ~static_cast<uintptr_t>(0);
	};
}
