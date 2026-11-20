///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: DataStateMachine.cpp
// Purpose: A state machine of a character, read from data - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "gameplay/DataStateMachine.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <set>

namespace Pakal
{
	bool DataStateMachine::load_json(const std::string& text, std::string* error)
	{
		try
		{
			return parse(text, error);
		}
		catch (const nlohmann::json::exception& e)
		{
			if (error) *error = std::string("not a state machine: ") + e.what();
			return false;
		}
	}

	bool DataStateMachine::parse(const std::string& text, std::string* error)
	{
		auto fail = [&](const std::string& why)
		{
			if (error) *error = why;
			return false;
		};
		const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
		if (!root.is_object() || !root.contains("states") || !root["states"].is_object() || root["states"].empty())
			return fail("not a state machine: no \"states\"");
		std::map<std::string, State> states;
		std::set<std::string> used;
		for (const auto& [id, json] : root["states"].items())
		{
			State s;
			s.name = json.value("name", id);
			s.onEnter = json.value("onEnter", std::string());
			s.onUpdate = json.value("onUpdate", std::string());
			for (const auto& t : json.value("transitions", nlohmann::json::array()))
				s.transitions.push_back({ t.value("if", std::string()), t.value("to", std::string()) });
			const nlohmann::json commands = json.value("commands", nlohmann::json::object());   // (kept: items() of a temporary dangles)
			for (const auto& [command, target] : commands.items())
			{
				s.commands[command] = target.get<std::string>();
				used.insert(command);
			}
			states[id] = std::move(s);
		}
		for (const auto& [id, s] : states)
		{
			for (const auto& t : s.transitions)
				if (!states.count(t.target)) return fail("state '" + id + "' goes to '" + t.target + "', which does not exist");
			for (const auto& [command, target] : s.commands)
				if (!states.count(target)) return fail("state '" + id + "': command " + command + " goes to '" + target + "', which does not exist");
		}
		const std::string initial = root.value("initial", std::string());
		if (!states.count(initial)) return fail("the initial state '" + initial + "' does not exist");
		m_states = std::move(states);
		m_initial = initial;
		m_current.clear();
		m_commands.clear();
		for (const auto& c : root.value("commands", nlohmann::json::array())) m_commands.push_back(c.get<std::string>());
		for (const std::string& c : used)
			if (std::find(m_commands.begin(), m_commands.end(), c) == m_commands.end()) m_commands.push_back(c);
		return true;
	}

	const DataStateMachine::State* DataStateMachine::state(const std::string& name) const
	{
		auto found = m_states.find(name);
		return found == m_states.end() ? nullptr : &found->second;
	}

	void DataStateMachine::enter(const std::string& name)
	{
		m_current = name;
		if (m_handler) m_handler(name, m_states[name].onEnter);
	}

	void DataStateMachine::start()
	{
		if (m_states.count(m_initial)) enter(m_initial);
	}

	bool DataStateMachine::process_command(const std::string& command)
	{
		const State* s = state(m_current);
		if (!s) return false;
		auto found = s->commands.find(command);
		if (found == s->commands.end()) return false;
		enter(found->second);
		return true;
	}

	void DataStateMachine::update()
	{
		const State* s = state(m_current);
		if (!s) return;
		if (!s->onUpdate.empty() && m_updateHandler) m_updateHandler(m_current, s->onUpdate);
		if (!m_condition) return;
		for (const Transition& t : s->transitions)
		{
			if (m_condition(t.condition))
			{
				enter(t.target);
				return;
			}
		}
	}

	bool DataStateMachine::force(const std::string& name)
	{
		if (!state(name)) return false;
		enter(name);
		return true;
	}

	std::vector<std::string> DataStateMachine::available_commands() const
	{
		std::vector<std::string> out;
		if (const State* s = state(m_current))
			for (const auto& entry : s->commands) out.push_back(entry.first);
		return out;
	}
}
