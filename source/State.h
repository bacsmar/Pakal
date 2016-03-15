#pragma once

#include "Config.h"
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

namespace Pakal {
	class State;
	class TransitionCondition;
	class StateMachine;
	class ScriptComponent;
	class Archive;
	/////////////////////////////////////////////////////////////////////////////////////////////////////////
	class  _PAKALExport  TransitionCondition
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
		TransitionCondition() : m_condition([]() { return false; })
		{
		}

		TransitionCondition(const std::function<bool()>& condition, State *final_state)
		{
			m_target_state = final_state;
			m_condition = condition;
		}
		TransitionCondition(const std::function<bool()>&& condition, State *final_state)
		{
			m_target_state = final_state;
			m_condition = condition;
		}
		inline State * get_target_state() const
		{
			return m_target_state;
		}
		inline bool execute() const
		{
			return is_enabled() && m_condition() ;
		}
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
		std::string on_update_str;

		std::vector<TransitionCondition>  m_transition_conditions;
		std::unordered_map<std::string, State*> m_transition_commands;

		explicit State(const std::string& name) : m_name(name){}
		explicit State() : event_enter([]() {}), event_exit([](){}), event_update([]()
		{
			
		}) {}

		~State(void) {}

		inline State* exec_conditions()
		{
			for (auto & transition : m_transition_conditions)
			{
				if (transition.execute())
				{
					return transition.get_target_state();
				}
			}
			return this;
		}		

		inline State* process_command(const std::string& command)
		{
			auto transition = m_transition_commands.find(command);
			return transition != m_transition_commands.end() ? transition->second: this;
		}
		
	public:
		std::function<void()> event_enter;
		std::function<void()> event_exit;
		std::function<void()> event_update;

		inline void add_transition_cmd(const std::string& command, State*  target_state)
		{			
			ASSERT(target_state != nullptr);			
			ASSERT(m_transition_commands.find(command) == m_transition_commands.end());
			m_transition_commands[command] = target_state;
		}
		
		inline const TransitionCondition& add_transition(const std::function<bool()>&& conditions, State * finalState)
		{
			ASSERT(finalState);
			m_transition_conditions.emplace_back(	 TransitionCondition(std::move(conditions), finalState ));
			return m_transition_conditions.back();
		}

		inline const std::string& get_name() const { return m_name; }

		void persist(Archive* archive);

		void set_script(ScriptComponent& script);		
	};

}