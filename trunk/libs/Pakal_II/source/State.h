#pragma once

#include <vector>
#include <functional>
#include <Event.h>

namespace Pakal {
	class State;
	class Transition;
	class StateMachine;

	class Transition
	{
		friend State;
		//State* m_initial_state;
		State* m_final_state;
		std::function<bool()> m_condition;
		bool m_enabled = true;
	public:
		inline bool is_enabled() const
		{
			return m_enabled;
		}
		inline void set_enabled(bool val)
		{
			m_enabled = val;
		}

	protected:
		Transition(State *initial_state, const std::function<bool()>& condition, State *final_state)
		{
			//m_initial_state = initial_state;
			m_final_state = final_state;
			m_condition = condition;
		}
		inline State * get_final_state()
		{
			return m_final_state;
		}
		inline bool execute()
		{
			return m_condition() && is_enabled();
		}
	};

	class State
	{
		friend StateMachine;
		std::vector<Transition>  m_transitions;

		State(void) {}
		~State(void) {}

		inline State *update()
		{
			for (auto & transition : m_transitions)
			{
				if (transition.execute())
				{
					return transition.get_final_state();
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
	};

}