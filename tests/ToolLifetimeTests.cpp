///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ToolLifetimeTests.cpp
// Purpose: The tool server's safety without sockets (handle_message): remove_tool returns only after a
//          running call of that tool ends (its owner may then free what the tool captured), a tool that
//          removes itself does not wait for itself, a replaced tool's running call finishes with the old
//          one; and the engine's tools never start in a players' copy (engine_tools_allowed).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "mcp/EngineTools.h"
#include "mcp/McpServer.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

using namespace Pakal;
using json = nlohmann::json;

namespace
{
	int g_failures = 0;

	void check(bool condition, const char* what)
	{
		std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", what);
		std::fflush(stdout);
		if (!condition) ++g_failures;
	}

	std::string call(McpServer& server, const std::string& name)
	{
		const json request = { { "jsonrpc", "2.0" }, { "id", 1 }, { "method", "tools/call" },
			{ "params", { { "name", name }, { "arguments", json::object() } } } };
		return server.handle_message(request.dump());
	}
}

int main()
{
	const json schema = { { "type", "object" } };

	// a running call holds its tool: remove_tool waits, then what it captured can go
	{
		McpServer server;
		auto owned = std::make_unique<int>(41);   // what a level owns and a tool reads
		std::atomic<bool> started{ false }, finished{ false };
		server.add_tool("slow", { "Reads what the level owns, slowly", schema, [&](const json&)
		{
			started = true;
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			const int value = *owned + 1;   // freed under it, this would read freed memory
			finished = true;
			return json{ { "value", value } };
		} });
		std::string answer;
		std::thread mcp([&]() { answer = call(server, "slow"); });
		while (!started) std::this_thread::sleep_for(std::chrono::milliseconds(1));
		server.remove_tool("slow");
		check(finished.load(), "remove_tool returns only after the running call of that tool ended");
		owned.reset();   // the level frees what the tool captured
		mcp.join();
		check(answer.find("42") != std::string::npos, "the running call finished with its answer");
		check(call(server, "slow").find("unknown tool") != std::string::npos, "once removed it is unknown");
	}

	// a tool that removes itself does not wait for itself
	{
		McpServer server;
		server.add_tool("once", { "Removes itself", schema, [&server](const json&)
		{
			server.remove_tool("once");
			return json{ { "gone", true } };
		} });
		std::atomic<bool> done{ false };
		std::thread mcp([&]() { call(server, "once"); done = true; });
		for (int i = 0; i < 200 && !done; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(5));
		check(done.load(), "a tool that removes itself returns (no wait for itself)");
		if (!done) mcp.detach(); else mcp.join();
	}

	// removing a tool that is not running, or one that does not exist, returns at once
	{
		McpServer server;
		server.add_tool("idle", { "Never called", schema, [](const json&) { return json::object(); } });
		const auto at = std::chrono::steady_clock::now();
		server.remove_tool("idle");
		server.remove_tool("nothing");
		check(std::chrono::steady_clock::now() - at < std::chrono::milliseconds(50), "removing an idle or unknown tool returns at once");
	}

	// a tool replaced while it runs: the running call ends with the old one, the next call gets the new one
	{
		McpServer server;
		std::atomic<bool> started{ false };
		std::atomic<int> oldEnded{ 0 }, newRan{ 0 };
		server.add_tool("v", { "Old", schema, [&](const json&)
		{
			started = true;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			++oldEnded;
			return json::object();
		} });
		std::thread mcp([&]() { call(server, "v"); });
		while (!started) std::this_thread::sleep_for(std::chrono::milliseconds(1));
		server.add_tool("v", { "New", schema, [&](const json&) { ++newRan; return json::object(); } });
		mcp.join();
		check(oldEnded == 1 && newRan == 0, "the running call ends with the tool it started");
		call(server, "v");
		check(newRan == 1, "the next call gets the new tool");
	}

	// a missing argument is the tool's error, with its schema, not the JSON library's exception
	{
		McpServer server;
		server.add_tool("shot", { "Needs a path", json{ { "type", "object" }, { "properties", { { "path", { { "type", "string" } } } } } },
			[](const json& arguments) { return json{ { "path", arguments.at("path").get<std::string>() } }; } });
		const json answer = json::parse(call(server, "shot"));
		const json result = answer.value("result", json::object());
		const std::string text = result.value("content", json::array()).empty() ? std::string()
			: result["content"][0].value("text", std::string());
		check(result.value("isError", false) && text.find("do not fit the tool") != std::string::npos
			&& text.find("json.exception") == std::string::npos && text.find("inputSchema") != std::string::npos,
			"a missing argument is a tool error with the schema, not a raw JSON exception");
	}

	// which requests are refused before their body is read: a web page's (an Origin, any case), one without the token
	{
		const std::string post = "POST /mcp HTTP/1.1\r\nHost: 127.0.0.1\r\n";
		const std::string bearer = "Authorization: Bearer secret\r\n";
		check(McpServer::refusal(post + bearer + "\r\n", "secret").empty(), "a request with the token is served");
		check(McpServer::refusal(post + "\r\n", "").empty(), "without a token set, a plain request is served");
		check(McpServer::refusal(post + bearer + "Origin: http://page.example\r\n\r\n", "secret") == "403 Forbidden",
			"a request with an Origin is refused (403), even with the token");
		check(McpServer::refusal(post + "origin: null\r\n\r\n", "") == "403 Forbidden", "... whatever the header's case");
		check(McpServer::refusal(post + "X-Original: 1\r\n\r\n", "").empty(), "a header that only starts like it is not an Origin");
		check(McpServer::refusal(post + "\r\n", "secret") == "401 Unauthorized", "without the bearer, refused (401)");
		check(McpServer::refusal(post + "Authorization: Bearer wrong\r\n\r\n", "secret") == "401 Unauthorized", "a wrong bearer, refused");
		check(McpServer::refusal("GET /mcp HTTP/1.1\r\n\r\n", "") == "404 Not Found", "not a POST to /mcp: 404");
	}

	// the engine's tools never start in a players' copy
	{
		std::string why;
		check(engine_tools_allowed(nullptr, "dev", why), "a dev build may start them");
		check(engine_tools_allowed("0", "qa", why), "a qa build may start them");
		check(!engine_tools_allowed("1", "dev", why) && !why.empty(), "not with PAKAL_RELEASE=1");
		check(!engine_tools_allowed(nullptr, "store", why) && !why.empty(), "not in a store build, whatever its environment");
	}

	std::printf("%s\n", g_failures ? "FAILED" : "ALL PASSED");
	return g_failures ? 1 : 0;
}
