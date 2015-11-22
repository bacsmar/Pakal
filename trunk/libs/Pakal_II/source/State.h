#pragma once

#include "Config.h"
#include <vector>
#include <functional>
#include <map>

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
		std::function<bool()> m_condition = []() { return false; };
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
		Transition()
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
	/////////////////////////////////////////////////////////////////////////////////////////////////////////
	class  _PAKALExport State
	{
		friend StateMachine;
		friend Archive;
	protected:
		std::string m_name;
		std::string on_enter_str;
		std::string on_exit_str;
		std::vector<Transition>  m_transitions;
		std::map<unsigned, State*> m_command_transitions;

		explicit State(const std::string& name) : m_name(name){}
		explicit State(){}

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
		inline State* process_command(unsigned command)
		{
			auto state = m_command_transitions.find(command);
			return state != m_command_transitions.end() ? state->second : this;
		}
	public:
		std::function<void()> event_enter = [](){}; //fired by StateMachine
		std::function<void()> event_exit = [](){};	//fired by StateMachine

		inline void add_transition_cmd(unsigned command, State*  finalState)
		{
			ASSERT(finalState);
			ASSERT(m_command_transitions.find(command) == m_command_transitions.end());
			m_command_transitions[command] = finalState;
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