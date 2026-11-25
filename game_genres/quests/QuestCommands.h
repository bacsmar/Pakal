///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: QuestCommands.h
// Purpose: A tester's commands to the quests over MCP (docs/QUESTS.md, the debug view; the studio's QA plan):
//          quests.signal, quests.start, quests.set_var, quests.set_stage, quests.abandon, quests.fail,
//          quests.save, quests.load, quests.validate, quests.bench. They arrive on the MCP server's thread but
//          the quests live in the simulation's: each command waits in a queue the game drains at its step
//          (drain), and the tool waits for its answer (a few seconds at most: no level, a paused game, an
//          error). Added only where the QA tools are (a development build with a token, never a release); the
//          queue is shared, so a call in flight while the level goes finds it closed, never freed memory.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "quests/QuestSystem.h"

#include <nlohmann/json.hpp>

#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>

namespace Pakal
{
	class McpServer;
}

namespace Pakal::Quests
{
	class CommandQueue
	{
	public:
		using Command = std::function<nlohmann::json(QuestSystem&)>;
		// From the MCP thread: the command carried out at the game's next step, its answer (or an error after timeoutMs)
		nlohmann::json run(Command command, int timeoutMs = 3000);
		// From the simulation's thread, at its step: every command waiting, in order
		void drain(QuestSystem& quests);
		// The level goes: what waits is answered with an error, and what comes later at once
		void close();

	private:
		struct Item { Command command; std::promise<nlohmann::json> answer; };
		std::mutex m_mutex;
		std::deque<std::shared_ptr<Item>> m_items;
		bool m_closed = false;
	};

	// The command tools on a server (the QA tools' rule: a token, not a release); false when not added
	bool add_command_tools(McpServer& server, std::shared_ptr<CommandQueue> queue, const char* token, const char* release);
	bool add_command_tools(std::shared_ptr<CommandQueue> queue);   // (on the server the host started)
	void remove_command_tools(McpServer& server);
	void remove_command_tools();
}
