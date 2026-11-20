///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: DataStateMachine.h
//
// Purpose: A state machine of a character, read from data: states with transitions that follow a condition (checked
//          each update) and commands that move to another state when the character is in the right one. The machine
//          knows no game: the conditions and what a state does on entering are the game's (two callbacks). It
//          is the script-free sibling of the engine's StateMachine (the original Automata: XML, bound to a Lua script): JSON, two callbacks, nothing to run a script.
//
//   {
//     "initial": "standing",
//     "commands": ["JUMP", "BLOCK"],                         (optional: else the ones the states use)
//     "states": {
//       "standing": { "onEnter": "standing_on_enter",         (what the game does: a name it knows)
//                     "onUpdate": "standing_on_update",       (optional: every update in the state)
//                     "transitions": [ { "if": "is_falling", "to": "falling" } ],
//                     "commands": { "JUMP": "jumping" } },
//       ...
//     }
//   }
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Platform.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace Pakal
{
	class _PAKALExport DataStateMachine
	{
	public:
		struct Transition
		{
			std::string condition;
			std::string target;
		};
		struct State
		{
			std::string name;
			std::string onEnter;
			std::string onUpdate;   // what the game does every update while in the state (optional)
			std::vector<Transition> transitions;
			std::map<std::string, std::string> commands;   // command -> target state
		};

		// False (and a reason in 'error') if the text is not a machine: no states, an initial or a target that does not exist
		bool load_json(const std::string& text, std::string* error = nullptr);

		// Whether a transition's condition holds now (asked in the order of the state's transitions)
		void set_condition(std::function<bool(const std::string& condition)> condition) { m_condition = std::move(condition); }
		// A state was entered ('onEnter' is empty if the state has none); called by start(), commands, updates and force()
		void set_handler(std::function<void(const std::string& state, const std::string& onEnter)> handler) { m_handler = std::move(handler); }

		// The current state has an onUpdate: called by every update(), before the transitions are looked at
		void set_update_handler(std::function<void(const std::string& state, const std::string& onUpdate)> handler) { m_updateHandler = std::move(handler); }

		// Enters the initial state
		void start();
		// The command of the current state: false if the state does not take it (the command is dropped)
		bool process_command(const std::string& command);
		// Follows the first transition of the current state whose condition holds (one per call, so a chain of
		// states takes one update each: a state always shows for a frame)
		void update();
		// Enters a state whatever the current one says
		bool force(const std::string& state);

		const std::string& current() const { return m_current; }
		const State* state(const std::string& name) const;
		const std::map<std::string, State>& states() const { return m_states; }
		// Every command of the machine
		const std::vector<std::string>& commands() const { return m_commands; }
		// The commands the current state takes
		std::vector<std::string> available_commands() const;

	private:
		bool parse(const std::string& text, std::string* error);
		void enter(const std::string& name);

		std::map<std::string, State> m_states;
		std::vector<std::string> m_commands;
		std::string m_initial, m_current;
		std::function<bool(const std::string&)> m_condition;
		std::function<void(const std::string&, const std::string&)> m_handler;
		std::function<void(const std::string&, const std::string&)> m_updateHandler;
	};
}
