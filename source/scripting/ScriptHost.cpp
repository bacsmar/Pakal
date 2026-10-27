///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ScriptHost.cpp
// Purpose: Runs a game's scripts (Lua) on its clock - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "scripting/ScriptHost.h"

#include "LogMgr.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <lua.hpp>

namespace Pakal
{
	namespace
	{
		// Run before anything else: Wait suspends the task on the game clock
		const char* const kPrelude = R"lua(
			function Wait(ms) __wait(ms) coroutine.yield() end
			function EnableSkip(on) __enableSkip(on and true or false) end
		)lua";
	}

	double ScriptArgs::number(size_t i, double fallback) const
	{
		if (i >= m_values.size()) return fallback;
		const ScriptValue& value = m_values[i];
		if (value.type == ScriptValue::Type::Number) return value.number;
		if (value.type == ScriptValue::Type::Text && !value.text.empty())
		{
			char* end = nullptr;
			const double parsed = std::strtod(value.text.c_str(), &end);
			if (end && *end == '\0') return parsed;
		}
		return fallback;
	}

	std::string ScriptArgs::text(size_t i, const std::string& fallback) const
	{
		if (i >= m_values.size()) return fallback;
		const ScriptValue& value = m_values[i];
		if (value.type == ScriptValue::Type::Text) return value.text;
		if (value.type == ScriptValue::Type::Number)
		{
			char buffer[64];
			if (std::floor(value.number) == value.number && std::fabs(value.number) < 1e15) std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value.number));
			else std::snprintf(buffer, sizeof(buffer), "%.14g", value.number);
			return buffer;
		}
		return fallback;
	}

	bool ScriptArgs::truth(size_t i) const
	{
		if (i >= m_values.size()) return false;
		const ScriptValue& value = m_values[i];
		if (value.type == ScriptValue::Type::Nil) return false;
		if (value.type == ScriptValue::Type::Boolean) return value.boolean;
		return true;
	}

	ScriptHost::ScriptHost()
	{
		m_lua = luaL_newstate();
		luaL_openlibs(m_lua);
		bind("__wait", [this](const ScriptArgs& args)
		{
			if (m_current)
			{
				m_current->resumeAt = m_clockMs + (m_skipping ? 0.0 : args.number(0));
			}
			return ScriptValue();
		});
		bind("__enableSkip", [this](const ScriptArgs& args)
		{
			m_skippable = args.truth(0);
			if (!m_skippable) m_skipping = false;
			return ScriptValue();
		});
		bind("SetGameVariable", [this](const ScriptArgs& args)
		{
			m_variables[args.text(0)] = args.number(1);
			return ScriptValue();
		});
		bind("GetGameVariable", [this](const ScriptArgs& args)
		{
			return ScriptValue::of(get_variable(args.text(0)));
		});
		run(kPrelude);
	}

	ScriptHost::~ScriptHost()
	{
		for (auto& task : m_tasks)
		{
			if (task->thread != 0)
			{
				luaL_unref(m_lua, LUA_REGISTRYINDEX, task->thread);
			}
		}
		lua_close(m_lua);
	}

	// Every bound function is this closure: the host and the function's index are its upvalues
	int ScriptHost::call_bound(lua_State* L)
	{
		ScriptHost* self = static_cast<ScriptHost*>(lua_touserdata(L, lua_upvalueindex(1)));
		const size_t index = static_cast<size_t>(lua_tointeger(L, lua_upvalueindex(2)));
		std::vector<ScriptValue> values;
		const int count = lua_gettop(L);
		values.reserve(static_cast<size_t>(count));
		for (int i = 1; i <= count; ++i)
		{
			ScriptValue value;
			switch (lua_type(L, i))
			{
			case LUA_TNUMBER: value = ScriptValue::of(lua_tonumber(L, i)); break;
			case LUA_TBOOLEAN: value = ScriptValue::of_bool(lua_toboolean(L, i) != 0); break;
			case LUA_TSTRING: value = ScriptValue::of_text(lua_tostring(L, i)); break;
			default: break; // nil, and what a function cannot take (tables, functions)
			}
			values.push_back(std::move(value));
		}
		const ScriptValue result = self->m_functions[index](ScriptArgs(std::move(values)));
		switch (result.type)
		{
		case ScriptValue::Type::Number: lua_pushnumber(L, result.number); return 1;
		case ScriptValue::Type::Boolean: lua_pushboolean(L, result.boolean ? 1 : 0); return 1;
		case ScriptValue::Type::Text: lua_pushstring(L, result.text.c_str()); return 1;
		default: return 0;
		}
	}

	void ScriptHost::bind(const std::string& name, ScriptFunction function)
	{
		m_functions.push_back(std::move(function));
		lua_pushlightuserdata(m_lua, this);
		lua_pushinteger(m_lua, static_cast<lua_Integer>(m_functions.size() - 1));
		lua_pushcclosure(m_lua, &ScriptHost::call_bound, 2);
		lua_setglobal(m_lua, name.c_str());
	}

	bool ScriptHost::run(const std::string& code)
	{
		if (luaL_dostring(m_lua, code.c_str()) != LUA_OK)
		{
			LOG_ERROR("[Script] %s", lua_tostring(m_lua, -1));
			lua_pop(m_lua, 1);
			return false;
		}
		return true;
	}

	bool ScriptHost::start(const std::string& name, const std::string& code, bool loop)
	{
		// Compile once here so a broken script is reported when it is loaded, not when it runs
		if (luaL_loadstring(m_lua, code.c_str()) != LUA_OK)
		{
			LOG_ERROR("[Script] '%s' does not compile: %s", name.c_str(), lua_tostring(m_lua, -1));
			lua_pop(m_lua, 1);
			return false;
		}
		lua_pop(m_lua, 1);

		auto task = std::make_unique<Task>();
		task->name = name;
		task->code = code;
		task->loop = loop;
		m_tasks.push_back(std::move(task));
		return true;
	}

	void ScriptHost::skip()
	{
		if (!m_skippable)
		{
			return;
		}
		m_skipping = true;
		for (auto& task : m_tasks) // what waits now is due at once
		{
			task->resumeAt = std::min(task->resumeAt, m_clockMs);
		}
	}

	void ScriptHost::update(double clockMs)
	{
		m_clockMs = clockMs;
		for (auto& task : m_tasks)
		{
			run_task(*task, clockMs);
		}
	}

	void ScriptHost::run_task(Task& task, double clockMs)
	{
		if (task.finished || clockMs < task.resumeAt)
		{
			return;
		}
		if (task.thread == 0)
		{
			// A fresh coroutine for the chunk (a loop task makes one per pass)
			lua_State* thread = lua_newthread(m_lua);
			task.thread = luaL_ref(m_lua, LUA_REGISTRYINDEX);
			task.state = thread;
			if (luaL_loadstring(thread, task.code.c_str()) != LUA_OK)
			{
				LOG_ERROR("[Script] '%s': %s", task.name.c_str(), lua_tostring(thread, -1));
				task.finished = true;
				return;
			}
		}

		m_current = &task;
		m_clockMs = clockMs;
		int results = 0;
		const int status = lua_resume(task.state, nullptr, 0, &results);
		m_current = nullptr;

		if (status == LUA_YIELD)
		{
			lua_settop(task.state, 0); // suspended by Wait: resumeAt is already set
			return;
		}
		if (status != LUA_OK)
		{
			LOG_ERROR("[Script] '%s' stopped: %s", task.name.c_str(), lua_tostring(task.state, -1));
			task.finished = true;
		}
		else if (!task.loop)
		{
			task.finished = true;
		}

		// The pass is over (or failed): the next one gets a new coroutine
		luaL_unref(m_lua, LUA_REGISTRYINDEX, task.thread);
		task.thread = 0;
		task.state = nullptr;
	}

	double ScriptHost::get_variable(const std::string& name) const
	{
		const auto found = m_variables.find(name);
		return found != m_variables.end() ? found->second : 0.0;
	}

	void ScriptHost::set_variable(const std::string& name, double value)
	{
		m_variables[name] = value;
	}

	void ScriptHost::stop(const std::string& name)
	{
		for (auto& task : m_tasks)
		{
			if (task->name != name || task->finished)
			{
				continue;
			}
			task->finished = true;
			if (task->thread != 0)
			{
				luaL_unref(m_lua, LUA_REGISTRYINDEX, task->thread);
				task->thread = 0;
				task->state = nullptr;
			}
		}
	}

	bool ScriptHost::is_running(const std::string& name) const
	{
		for (const auto& task : m_tasks)
		{
			if (task->name == name && !task->finished)
			{
				return true;
			}
		}
		return false;
	}
}
