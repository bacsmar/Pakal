#pragma once

#include "Config.h"

#include "State.h"
#include <set>

namespace Pakal
{
	class  StateMachine
	{
		std::set<State*> m_States;
		State *currentState = nullptr;
	public:
		StateMachine(void);
		virtual ~StateMachine(void);

		inline void		set_current_state(State *s) { currentState = s; }
		inline State*	get_current_state() const { return currentState; }

		State*	create_state();
		void	remove_state(State *);

		size_t	size() const;
		// range-based for
		decltype(m_States)::iterator	begin();
		decltype(m_States)::iterator	end();

		void update();
	};

}
