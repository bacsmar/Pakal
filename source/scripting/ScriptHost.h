///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ScriptHost.h
// Purpose: Runs a game's scripts (Lua) on its clock. A script is a set of tasks: each one is a Lua
//          coroutine, and Wait(ms) suspends it on the game clock (not the wall clock), so scripts play
//          the same with the simulation. A game binds its own functions by name; they see their
//          arguments as ScriptArgs and give back a ScriptValue, never a Lua type. Built in: Wait,
//          EnableSkip, GetGameVariable and SetGameVariable. A library of its own (PakalScripting):
//          the engine does not depend on Lua.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct lua_State;

namespace Pakal
{
	// A value between a script and the game: nothing, a number, a boolean or text
	struct ScriptValue
	{
		enum class Type { Nil, Number, Boolean, Text };
		Type type = Type::Nil;
		double number = 0.0;
		bool boolean = false;
		std::string text;

		static ScriptValue of(double value) { ScriptValue v; v.type = Type::Number; v.number = value; return v; }
		static ScriptValue of_bool(bool value) { ScriptValue v; v.type = Type::Boolean; v.boolean = value; return v; }
		static ScriptValue of_text(std::string value) { ScriptValue v; v.type = Type::Text; v.text = std::move(value); return v; }
	};

	// The arguments of a bound function, read as Lua reads them (0-based here)
	class ScriptArgs
	{
	public:
		explicit ScriptArgs(std::vector<ScriptValue> values) : m_values(std::move(values)) {}

		size_t size() const { return m_values.size(); }
		bool is_number(size_t i) const { return i < m_values.size() && m_values[i].type == ScriptValue::Type::Number; }
		bool is_boolean(size_t i) const { return i < m_values.size() && m_values[i].type == ScriptValue::Type::Boolean; }
		// Text or a number (Lua's lua_isstring)
		bool is_text(size_t i) const { return i < m_values.size() && (m_values[i].type == ScriptValue::Type::Text || m_values[i].type == ScriptValue::Type::Number); }

		// The number (a numeric text too), or 'fallback' when it is missing or not one
		double number(size_t i, double fallback = 0.0) const;
		// The text (a number as Lua writes it), or 'fallback'
		std::string text(size_t i, const std::string& fallback = std::string()) const;
		// Lua's truth: only nothing and false are false
		bool truth(size_t i) const;

	private:
		std::vector<ScriptValue> m_values;
	};

	using ScriptFunction = std::function<ScriptValue(const ScriptArgs& args)>;

	class ScriptHost
	{
	public:
		ScriptHost();
		~ScriptHost();
		ScriptHost(const ScriptHost&) = delete;
		ScriptHost& operator=(const ScriptHost&) = delete;

		// A function the scripts can call by this name
		void bind(const std::string& name, ScriptFunction function);
		// Runs Lua code once now (definitions the game's scripts use). False when it fails (logged).
		bool run(const std::string& code);

		// Starts a task from a Lua chunk. A looping task runs its chunk again each time it finishes. Returns false when the
		// chunk does not compile (logged).
		bool start(const std::string& name, const std::string& code, bool loop);
		// Resumes every task that is due at game time 'clockMs' (milliseconds)
		void update(double clockMs);

		// Game variables shared with the scripts (GetGameVariable / SetGameVariable)
		double get_variable(const std::string& name) const;
		void set_variable(const std::string& name, double value);

		// EnableSkip(true) lets the player cut a scene short: skip() makes every Wait pass at once until EnableSkip(false)
		bool skippable() const { return m_skippable; }
		bool skipping() const { return m_skipping; }
		void skip();

		bool is_running(const std::string& name) const;
		// Ends every task with this name (its coroutine is dropped; a suspended Wait is never resumed)
		void stop(const std::string& name);
		size_t task_count() const { return m_tasks.size(); }

	private:
		struct Task
		{
			std::string name;
			std::string code;
			bool loop = false;
			bool finished = false;
			double resumeAt = 0.0;  // game time (ms) before which the task stays suspended
			int thread = 0;         // registry reference of the coroutine, 0 when not running
			lua_State* state = nullptr;
		};

		static int call_bound(lua_State* L);
		void run_task(Task& task, double clockMs);

		lua_State* m_lua = nullptr;
		std::vector<ScriptFunction> m_functions;
		std::vector<std::unique_ptr<Task>> m_tasks;
		std::map<std::string, double> m_variables;
		double m_clockMs = 0.0;
		Task* m_current = nullptr; // the task being resumed (Wait suspends it)
		bool m_skippable = false;
		bool m_skipping = false;
	};
}
