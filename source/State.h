#pragma once

#include "Config.h"
#include <vector>
#include <functional>
//#include <map>
#include <algorithm>
#include "Utils.h"

namespace Pakal {
	class State;
	class Transition;
	class StateMachine;
	class ScriptComponent;
	class Archive;
	/////////////////////////////////////////////////////////////////////////////////////////////////////////
	class  _PAKALExport  Transition
	{
		friend class State;
		friend Archive;	

		State* m_target_state = nullptr;
		bool m_enabled = true;
		std::function<bool()> m_condition;
		std::string fn_condition;
	public:
		inline bool is_enabled() const
		{
			return m_enabled;
		}
		inline void set_enabled(bool val)
		{
			m_enabled = val;
		}

		inline void set_condition(const std::function<bool()>& condition)
		{
			m_condition = condition;
		}

		void persist(Archive* archive);

		void set_script(ScriptComponent& script);

	protected:
		Transition() : m_condition([]() { return false; })
		{
		}

		Transition(const std::function<bool()>& condition, State *final_state)
		{
			m_target_state = final_state;
			m_condition = condition;
		}
		Transition(const std::function<bool()>&& condition, State *final_state)
		{
			m_target_state = final_state;
			m_condition = condition;
		}
		inline State * get_target_state() const
		{
			return m_target_state;
		}
		inline bool execute()
		{
			return is_enabled() && m_condition() ;
		}
	};

	struct TransitionCommand
	{
		uint32_t command_hash;
		State* target_state;
		std::string command_name;		
		void persist(Archive* archive);
	};

	/////////////////////////////////////////////////////////////////////////////////////////////////////////
	class  _PAKALExport State
	{
		friend StateMachine;
		friend Archive;
		friend std::is_constructible<State>;
	protected:
		std::string m_name;
		std::string on_enter_str;
		std::string on_exit_str;
		std::vector<Transition>  m_transitions;

		std::vector<TransitionCommand> m_commands;
		//std::map<unsigned, State*> m_command_transitions;

		explicit State(const std::string& name) : m_name(name){}
		explicit State() : event_enter([]() {}), event_exit([](){}){}

		~State(void) {}

		inline State* update()
		{
			for (auto & transition : m_transitions)
			{
				if (transition.execute())
				{
					return transition.get_target_state();
				}
			}
			return this;
		}
		inline State* process_command(int command)
		{
			// TODO: LUISITO arregla los mapas! en el persist
			//auto state = m_command_transitions.find(command);
			//return state != m_command_transitions.end() ? state->second : this;

			auto state = std::find_if(m_commands.begin(), m_commands.end(), [command](const TransitionCommand& c)
			{
				return c.command_hash == command;
			} );
			return state != m_commands.end() ? (*state).target_state : this;
		}

		inline State* process_command(const std::string& command)
		{
			return process_command( crypt_utils::hash_joaat(command) );
		}
		
	public:
		std::function<void()> event_enter;
		std::function<void()> event_exit;

		inline void add_transition_cmd(const std::string& command, State*  target_state)
		{
			ASSERT(target_state);
			//ASSERT(m_command_transitions.find(command) == m_command_transitions.end());
			//m_command_transitions[command] = finalState;

			//m_commands.push_back({command,finalState});
			auto command_hash = Pakal::crypt_utils::hash_joaat(command);
			m_commands.emplace_back(TransitionCommand{ command_hash, target_state, command });
		}

		inline const Transition& add_transition(const std::function<bool()>& conditions, State * finalState)
		{
			ASSERT(finalState);
			m_transitions.emplace_back(Transition{ std::move(conditions), finalState });
			return m_transitions.back();
		}
		inline const Transition& add_transition(const std::function<bool()>&& conditions, State * finalState)
		{
			ASSERT(finalState);
			m_transitions.emplace_back(	 Transition(std::move(conditions), finalState ));
			return m_transitions.back();
		}

		inline const std::string& get_name() const { return m_name; }

		void persist(Archive* archive);

		void set_script(ScriptComponent& script);		
	};

}