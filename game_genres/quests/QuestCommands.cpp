#include "quests/QuestCommands.h"

// The MCP server is in every desktop build, and in an Android one only with the engine's tools (build_apk.py --tools,
// PAKAL_ENGINE_TOOLS): a store build has none, so there the tools are not added (and nothing here links against it)
#if !defined(__ANDROID__) || defined(PAKAL_ENGINE_TOOLS)

#include "mcp/McpServer.h"
#include "mcp/QaTools.h"

#include <chrono>
#include <cstdlib>
#include <stdexcept>

namespace Pakal::Quests
{
	nlohmann::json CommandQueue::run(Command command, int timeoutMs)
	{
		auto item = std::make_shared<Item>();
		item->command = std::move(command);
		std::future<nlohmann::json> answer = item->answer.get_future();
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_closed) throw std::runtime_error("no level runs the quests now");
			m_items.push_back(item);
		}
		if (answer.wait_for(std::chrono::milliseconds(timeoutMs)) != std::future_status::ready)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			for (auto it = m_items.begin(); it != m_items.end(); ++it)
				if (*it == item) { m_items.erase(it); break; }
			throw std::runtime_error("the game did not take the command in time (paused, or loading)");
		}
		return answer.get();
	}

	void CommandQueue::drain(QuestSystem& quests)
	{
		std::deque<std::shared_ptr<Item>> items;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			items.swap(m_items);
		}
		for (auto& item : items)
		{
			try { item->answer.set_value(item->command(quests)); }
			catch (const std::exception& e) { item->answer.set_value({ { "error", e.what() } }); }
		}
	}

	void CommandQueue::close()
	{
		std::deque<std::shared_ptr<Item>> items;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_closed = true;
			items.swap(m_items);
		}
		for (auto& item : items) item->answer.set_value({ { "error", "the level is gone" } });
	}

	namespace
	{
		Ref ref_of(const nlohmann::json& args, const char* kindKey, const char* idKey)
		{
			return Ref{ args.value(kindKey, std::string("place")), args.value(idKey, static_cast<uint64_t>(0)) };
		}

		nlohmann::json schema(std::initializer_list<std::pair<const char*, const char*>> fields)
		{
			nlohmann::json properties = nlohmann::json::object();
			for (const auto& [name, type] : fields) properties[name] = { { "type", type } };
			return { { "type", "object" }, { "properties", properties } };
		}
	}

	bool add_command_tools(McpServer& server, std::shared_ptr<CommandQueue> queue, const char* token, const char* release)
	{
		std::string why;
		if (!qa_tools_allowed(token, release, why)) return false;
		auto tool = [&server, queue](const char* name, const char* about, nlohmann::json inputSchema,
			std::function<nlohmann::json(QuestSystem&, const nlohmann::json&)> body)
		{
			server.add_tool(name, { about, inputSchema, [queue, body](const nlohmann::json& args)
			{
				// (arguments that do not fit the tool's schema: an answer saying so, never a raw exception)
				if (!args.is_object()) return nlohmann::json{ { "error", "the arguments are not an object" } };
				return queue->run([body, args](QuestSystem& q)
				{
					try { return body(q, args); }
					catch (const std::exception& e) { return nlohmann::json{ { "error", std::string("the arguments do not fit the tool's schema: ") + e.what() } }; }
				});
			} });
		};
		tool("quests.signal", "Sends a signal to the quests, as the game would (logged with its subject): name, kind, id",
			schema({ { "name", "string" }, { "kind", "string" }, { "id", "integer" } }),
			[](QuestSystem& q, const nlohmann::json& a) { q.signal(a.value("name", std::string()), ref_of(a, "kind", "id")); return nlohmann::json{ { "queued", true } }; });
		tool("quests.start", "Starts a record now, its aliases given as {name: {kind, id}} or filled from the world; returns its serial (0: not started)",
			schema({ { "record", "string" }, { "aliases", "object" } }),
			[](QuestSystem& q, const nlohmann::json& a)
		{
			std::map<std::string, Ref> given;
			const nlohmann::json aliases = a.value("aliases", nlohmann::json::object());
			for (auto it = aliases.begin(); it != aliases.end(); ++it) given[it.key()] = ref_of(it.value(), "kind", "id");
			return nlohmann::json{ { "serial", q.start(a.value("record", std::string()), "qa", given) } };
		});
		tool("quests.set_var", "Sets a running quest's var (its 'when' exits read them): serial, name, value",
			schema({ { "serial", "integer" }, { "name", "string" }, { "value", "number" } }),
			[](QuestSystem& q, const nlohmann::json& a) { q.set_var(a.value("serial", static_cast<uint64_t>(0)), a.value("name", std::string()), a.value("value", 0.0)); return nlohmann::json{ { "set", true } }; });
		tool("quests.set_stage", "Moves a running quest to a stage of its record at the next step (logged with cause qa): serial, stage",
			schema({ { "serial", "integer" }, { "stage", "integer" } }),
			[](QuestSystem& q, const nlohmann::json& a) { q.force_stage(a.value("serial", static_cast<uint64_t>(0)), a.value("stage", 0)); return nlohmann::json{ { "queued", true } }; });
		tool("quests.restart_checkpoint", "Restarts a running quest from its last checkpoint stage (its start if none) at the next step: its spawns made anew: serial",
			schema({ { "serial", "integer" } }),
			[](QuestSystem& q, const nlohmann::json& a) { q.restart_checkpoint(a.value("serial", static_cast<uint64_t>(0))); return nlohmann::json{ { "queued", true } }; });
		tool("quests.kill_target", "Downs a running quest's target as a hit would: serial, alias (\"\": every spawn of it still standing), "
			"by (\"world\": a protected alias is refused; \"player\": it is not), count (at most so many spawns). Answers killed, protected, refused",
			schema({ { "serial", "integer" }, { "alias", "string" }, { "by", "string" }, { "count", "integer" } }),
			[](QuestSystem& q, const nlohmann::json& a)
		{
			return q.kill_target(a.value("serial", static_cast<uint64_t>(0)), a.value("alias", std::string()), a.value("by", std::string("world")), a.value("count", -1));
		});
		tool("quests.teleport_to_alias", "Puts the player 'meters' (default 6) from a running quest's alias's target (\"\": its first spawn standing): "
			"serial, alias, meters. The bot's teleport for open worlds: a route counts when the stage's real exit fires",
			schema({ { "serial", "integer" }, { "alias", "string" }, { "meters", "number" } }),
			[](QuestSystem& q, const nlohmann::json& a)
		{
			return q.teleport_to_alias(a.value("serial", static_cast<uint64_t>(0)), a.value("alias", std::string()), a.value("meters", 6.0));
		});
		tool("quests.bring_player", "Puts the player 'meters' (default 6) from a target with no quest needed (R-order: done before the quest starts): kind, id, meters",
			schema({ { "kind", "string" }, { "id", "integer" }, { "meters", "number" } }),
			[](QuestSystem& q, const nlohmann::json& a) { return q.bring_player(ref_of(a, "kind", "id"), a.value("meters", 6.0)); });
		tool("quests.set_fact", "Puts (on true, the default) or takes a world fact of a target as the world would (a stage that waits on "
			"the world, played by a route): kind, id, name, on. Answers set (false: the game cannot)",
			schema({ { "kind", "string" }, { "id", "integer" }, { "name", "string" }, { "on", "boolean" } }),
			[](QuestSystem& q, const nlohmann::json& a) { return q.set_fact(ref_of(a, "kind", "id"), a.value("name", std::string()), a.value("on", true)); });
		tool("quests.journal", "The journal: every quest kept, newest first: serial, record, title, state, stage, objectives, waits (what a running "
			"quest's exits wait on, one line each)", schema({}),
			[](QuestSystem& q, const nlohmann::json&) { return nlohmann::json{ { "entries", q.journal() } }; });
		tool("quests.use", "Makes a use as the player would (an exit's 'use': talk, pick up, install), credited at the next step if it is offered "
			"then: serial, stage and exit (left out: the quest's first use offered now). Answers queued and the offer, or why there is none",
			schema({ { "serial", "integer" }, { "stage", "integer" }, { "exit", "integer" } }),
			[](QuestSystem& q, const nlohmann::json& a)
		{
			const uint64_t serial = a.value("serial", static_cast<uint64_t>(0));
			for (const QuestSystem::Offer& o : q.offers())
			{
				if (o.serial != serial) continue;
				if (a.contains("exit") && (o.exit != a.value("exit", -1) || o.stage != a.value("stage", o.stage))) continue;
				q.use(o.serial, o.stage, o.exit);
				return nlohmann::json{ { "queued", true }, { "stage", o.stage }, { "exit", o.exit }, { "alias", o.alias }, { "action", o.action }, { "distance", o.distance } };
			}
			return nlohmann::json{ { "queued", false }, { "why", "no use of that quest is offered now (too far, items missing, or its other conditions)" } };
		});
		tool("quests.items", "The player's quest items (name: count) and the uses offered now", schema({}),
			[](QuestSystem& q, const nlohmann::json&)
		{
			nlohmann::json offers = nlohmann::json::array();
			for (const QuestSystem::Offer& o : q.offers())
				offers.push_back({ { "serial", o.serial }, { "stage", o.stage }, { "exit", o.exit }, { "alias", o.alias }, { "action", o.action }, { "distance", o.distance } });
			return nlohmann::json{ { "items", q.items() }, { "offers", offers } };
		});
		tool("quests.choices", "The choices the player is asked now: serial, stage, prompt, speaker, options (id, text); and the ones answered "
			"(record:stage -> option)", schema({}),
			[](QuestSystem& q, const nlohmann::json&)
		{
			nlohmann::json asked = nlohmann::json::array();
			for (const QuestSystem::Choice& c : q.choices())
			{
				nlohmann::json options = nlohmann::json::array();
				for (const auto& o : c.options) options.push_back({ { "id", o.id }, { "text", o.text } });
				asked.push_back({ { "serial", c.serial }, { "stage", c.stage }, { "prompt", c.prompt }, { "speaker", c.speakerAlias }, { "options", options } });
			}
			return nlohmann::json{ { "asked", asked }, { "chosen", q.save().value("chosen", nlohmann::json::object()) } };
		});
		tool("quests.choose", "Picks an option of a choice as the player would, at the next step if it is still offered then: serial, option "
			"(stage left out: the quest's current one)", schema({ { "serial", "integer" }, { "stage", "integer" }, { "option", "string" } }),
			[](QuestSystem& q, const nlohmann::json& a)
		{
			const uint64_t serial = a.value("serial", static_cast<uint64_t>(0));
			const std::string option = a.value("option", std::string());
			for (const QuestSystem::Choice& c : q.choices())
			{
				if (c.serial != serial || (a.contains("stage") && a.value("stage", 0) != c.stage)) continue;
				for (const auto& o : c.options)
					if (o.id == option) { q.choose(serial, c.stage, option); return nlohmann::json{ { "queued", true }, { "stage", c.stage } }; }
				return nlohmann::json{ { "queued", false }, { "why", "no option '" + option + "' offered in that choice" } };
			}
			return nlohmann::json{ { "queued", false }, { "why", "that quest asks no choice now" } };
		});
		tool("quests.set_item", "Sets how many of a quest item the player carries (0: none), as a tester: name, count",
			schema({ { "name", "string" }, { "count", "integer" } }),
			[](QuestSystem& q, const nlohmann::json& a) { q.set_item(a.value("name", std::string()), a.value("count", 1)); return nlohmann::json{ { "set", true } }; });
		tool("quests.set_flag", "Sets a flag of the game's store (through the binding), as the game would: name, value", schema({ { "name", "string" }, { "value", "number" } }),
			[](QuestSystem& q, const nlohmann::json& a) { q.set_flag(a.value("name", std::string()), a.value("value", 1.0)); return nlohmann::json{ { "set", true } }; });
		tool("quests.abandon", "Abandons a running quest (a story quest refuses): serial", schema({ { "serial", "integer" } }),
			[](QuestSystem& q, const nlohmann::json& a) { q.abandon(a.value("serial", static_cast<uint64_t>(0))); return nlohmann::json{ { "queued", true } }; });
		tool("quests.fail", "Fails a running quest (a story quest retries or hands over): serial, reason",
			schema({ { "serial", "integer" }, { "reason", "string" } }),
			[](QuestSystem& q, const nlohmann::json& a) { q.fail(a.value("serial", static_cast<uint64_t>(0)), a.value("reason", std::string("qa"))); return nlohmann::json{ { "queued", true } }; });
		tool("quests.save", "The quests' state as they save it (format 1)", schema({}),
			[](QuestSystem& q, const nlohmann::json&) { return q.save(); });
		tool("quests.load", "Loads a state given as 'state' (what quests.save returned), at a step's boundary", schema({ { "state", "object" } }),
			[](QuestSystem& q, const nlohmann::json& a)
		{
			std::vector<std::string> errors;
			const bool ok = q.load(a.value("state", nlohmann::json::object()), errors);
			return nlohmann::json{ { "loaded", ok }, { "errors", errors } };
		});
		tool("quests.transitions", "The quest system's transitions after a sequence number, oldest first: {seq, step, serial, record, "
			"from, to, outcome (start, next, completed, failed, abandoned, retry, refill, refused), cause}", schema({ { "since", "integer" } }),
			[](QuestSystem& q, const nlohmann::json& a)
		{
			const uint64_t since = a.contains("since") && a["since"].is_number_integer() && a["since"].get<int64_t>() > 0 ? static_cast<uint64_t>(a["since"].get<int64_t>()) : 0;
			nlohmann::json entries = nlohmann::json::array();
			for (const nlohmann::json& e : q.log()) if (e.value("seq", static_cast<uint64_t>(0)) > since) entries.push_back(e);
			return nlohmann::json{ { "entries", entries } };
		});
		tool("quests.validate", "Every record checked against the settings (the catalog, stages reachable and ending): the findings", schema({}),
			[](QuestSystem& q, const nlohmann::json&)
		{
			nlohmann::json findings = nlohmann::json::array();
			for (const auto& [id, record] : q.records())
				for (const std::string& f : check_record(record, q.settings(), {})) findings.push_back(f);
			for (const std::string& f : check_chains(q.records())) findings.push_back(f);
			return nlohmann::json{ { "records", q.records().size() }, { "findings", findings } };
		});
		tool("quests.bench", "The quests' step time over the last steps (median, p95, max in ms) and the quests running", schema({}),
			[](QuestSystem& q, const nlohmann::json&) { return q.step_times(); });
		return true;
	}

	bool add_command_tools(std::shared_ptr<CommandQueue> queue)
	{
		McpServer* server = McpServer::active();
		return server && add_command_tools(*server, std::move(queue), std::getenv("PAKAL_MCP_TOKEN"), std::getenv("PAKAL_RELEASE"));
	}

	void remove_command_tools(McpServer& server)
	{
		for (const char* name : { "quests.signal", "quests.start", "quests.set_var", "quests.set_stage", "quests.abandon", "quests.fail",
			"quests.save", "quests.load", "quests.transitions", "quests.restart_checkpoint", "quests.kill_target", "quests.teleport_to_alias", "quests.bring_player", "quests.set_fact", "quests.journal", "quests.use", "quests.items", "quests.set_item", "quests.choices", "quests.choose", "quests.set_flag", "quests.validate", "quests.bench" })
			server.remove_tool(name);
	}

	void remove_command_tools()
	{
		if (McpServer* server = McpServer::active()) remove_command_tools(*server);
	}
}
#else
namespace Pakal::Quests
{
	nlohmann::json CommandQueue::run(Command, int) { return { { "error", "no tools in this build" } }; }
	void CommandQueue::drain(QuestSystem&) {}
	void CommandQueue::close() {}
	bool add_command_tools(McpServer&, std::shared_ptr<CommandQueue>, const char*, const char*) { return false; }
	bool add_command_tools(std::shared_ptr<CommandQueue>) { return false; }
	void remove_command_tools(McpServer&) {}
	void remove_command_tools() {}
}
#endif
