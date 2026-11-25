#include "quests/QuestTools.h"

// The MCP server is in every desktop build, and in an Android one only with the engine's tools (build_apk.py --tools,
// PAKAL_ENGINE_TOOLS): a store build has none, so there the tools are not added (and nothing here links against it)
#if !defined(__ANDROID__) || defined(PAKAL_ENGINE_TOOLS)

#include "mcp/McpServer.h"
#include "mcp/QaTools.h"

#include <cstdlib>

namespace Pakal::Quests
{
	bool add_tools(McpServer& server, const Arbiter& arbiter, const char* token, const char* release)
	{
		std::string why;
		if (!qa_tools_allowed(token, release, why)) return false;
		// (the shared block, not the arbiter: a call in flight keeps it alive after the level and its arbiter are gone)
		std::shared_ptr<const Published> shared = arbiter.shared();
		const nlohmann::json none = { { "type", "object" }, { "properties", nlohmann::json::object() } };
		server.add_tool("quests.state", { "The quests' state: every claim (owner, priority, marker, secondary marks, objective lines), the "
			"tracked owner and what is shown (the presentation and its revision); 'game': the game's own (its quests, flags)",
			none, [shared](const nlohmann::json&) { return state_of(*shared); } });
		const nlohmann::json since = { { "type", "object" }, { "properties", { { "since", { { "type", "integer" },
			{ "description", "the last sequence number already read (0: from the start of what is kept)" } } } } } };
		server.add_tool("quests.log", { "The quests' decisions after a sequence number, oldest first: {seq, step, event (claim, release, "
			"track, shown), owner, detail}", since, [shared](const nlohmann::json& arguments)
		{
			return nlohmann::json{ { "entries", log_of(*shared, arguments.value("since", static_cast<uint64_t>(0))) } };
		} });
		return true;
	}

	bool add_tools(const Arbiter& arbiter)
	{
		McpServer* server = McpServer::active();
		return server && add_tools(*server, arbiter, std::getenv("PAKAL_MCP_TOKEN"), std::getenv("PAKAL_RELEASE"));
	}

	void remove_tools(McpServer& server)
	{
		server.remove_tool("quests.state");
		server.remove_tool("quests.log");
	}

	void remove_tools()
	{
		if (McpServer* server = McpServer::active()) remove_tools(*server);
	}
}
#else
namespace Pakal::Quests
{
	bool add_tools(McpServer&, const Arbiter&, const char*, const char*) { return false; }
	bool add_tools(const Arbiter&) { return false; }
	void remove_tools(McpServer&) {}
	void remove_tools() {}
}
#endif
