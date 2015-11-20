#pragma once

#include <vector>
#include <functional>
#include <Event.h>

namespace Pakal {
	class State;
	class Transition;
	class StateMachine;
	class ScriptComponent;
	class Archive;
	/////////////////////////////////////////////////////////////////////////////////////////////////////////
	class Transition
	{
		friend State;
		friend Archive;
		//State* m_initial_state;
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
		Transition()
		{
		}
		Transition(State *initial_state, const std::function<bool()>& condition, State *final_state)
		{
			//m_initial_state = initial_state;
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
	class State
	{
		friend StateMachine;
		friend Archive;
	protected:
		std::string m_name;
		std::vector<Transition>  m_transitions;
		std::string on_enter_str;
		std::string on_exit_str;

		explicit State(const std::string& name) : m_name(name){}
		explicit State(){}

		~State(void) {}

		inline State *update()
		{
			for (auto & transition : m_transitions)
			{
				if (transition.execute())
				{
					return transition.get_target_state();
				}
			}
			//event_update.notify();
			return this;
		}
	public:

		//Pakal::Event<void> event_update;
		Pakal::Event<void> event_enter;	// fired by StateMachine
		Pakal::Event<void> event_exit;	// fired by StateMachine

		inline const Transition& add_transition(const std::function<bool()>& conditions, State * finalState)
		{
			m_transitions.emplace_back(Transition{ this, conditions, finalState });
			return m_transitions.back();
		}

		inline const std::string& get_name() const { return m_name; }

		void persist(Archive* archive);

		void set_script(ScriptComponent& script);		
	};

}