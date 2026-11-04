///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: McpServerTests.cpp
// Purpose: The MCP server over its socket: it answers tools/list and tools/call from another thread, a tool
//          added after it started is called, a token is required when set, and McpClient (the editor's)
//          reads the answers.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "mcp/McpClient.h"
#include "mcp/McpServer.h"

#include <cstdio>

using namespace Pakal;
using json = nlohmann::json;

namespace
{
	int g_failures = 0;

	void check(bool condition, const char* what)
	{
		if (!condition)
		{
			++g_failures;
			std::printf("FAILED: %s\n", what);
		}
	}
}

int main()
{
	const unsigned short port = 18765;
	McpServer server;
	server.add_tool("echo", { "Says it back", json{ { "type", "object" } }, [](const json& arguments) { return json{ { "said", arguments.value("text", std::string()) } }; } });
	std::string error;
	check(server.start(port, "secret", error), "the server starts");
	check(McpServer::active() == &server, "it is the active one");
	server.add_tool("late", { "Added after the start", json{ { "type", "object" } }, [](const json&) { return json{ { "late", true } }; } });

	McpClient client(port, "secret");
	json tools;
	check(client.list_tools(tools, error) && tools.is_array() && tools.size() == 2, "tools/list answers over the socket");
	json out;
	check(client.call("echo", { { "text", "hello" } }, out, error) && out.value("said", std::string()) == "hello", "a tool is called");
	check(client.call("late", json::object(), out, error) && out.value("late", false), "a tool added after the start is called");
	check(!client.call("nothing", json::object(), out, error) && !error.empty(), "an unknown tool is an error");

	McpClient stranger(port, "");
	check(!stranger.call("echo", json::object(), out, error), "without the token it is refused");

	server.stop();
	check(McpServer::active() == nullptr, "stopped, none is active");
	check(!client.call("echo", json::object(), out, error), "a stopped server does not answer");
	if (g_failures == 0) std::printf("McpServerTests: all passed\n");
	return g_failures == 0 ? 0 : 1;
}
