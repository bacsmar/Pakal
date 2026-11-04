///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: McpClient.h
// Purpose: A client of an MCP server on this machine (127.0.0.1:<port>: a game's, or a device's through
//          `adb forward`): JSON-RPC over HTTP, one blocking request at a time, with a timeout. What the
//          level editor's Device panel uses (from a thread of its own); tools/pakal_device.py does the same
//          in Python.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Platform.h"

#include <nlohmann/json.hpp>

#include <string>

namespace Pakal
{
	class _PAKALExport McpClient
	{
	public:
		McpClient(unsigned short port = 8765, std::string token = std::string(), double timeoutSeconds = 5.0)
			: m_port(port), m_token(std::move(token)), m_timeout(timeoutSeconds) {}

		void set_port(unsigned short port) { m_port = port; }
		unsigned short port() const { return m_port; }

		// One JSON-RPC call: its "result" (false and 'error' when it could not be reached or answered an error)
		bool rpc(const std::string& method, const nlohmann::json& params, nlohmann::json& result, std::string& error);
		// The tools ([{name, description, inputSchema}]) and a tool's answer (its JSON)
		bool list_tools(nlohmann::json& tools, std::string& error);
		bool call(const std::string& tool, const nlohmann::json& arguments, nlohmann::json& out, std::string& error);

	private:
		unsigned short m_port;
		std::string m_token;
		double m_timeout;
		int m_nextId = 1;
	};
}
