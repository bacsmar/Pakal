#include "StateMachine.h"
#include "State.h"
#include "LogMgr.h"

using namespace Pakal;

StateMachine::StateMachine(void)
{	
}


StateMachine::~StateMachine(void)
{
	for(auto &it : m_States)
	{
		delete it;
	}
	m_States.clear();
}

void StateMachine::update()
{	
	ASSERT(currentState);	// did you forget to set set_current_state(initialState)?
	State *newState = currentState->update();
	if( newState != currentState)
	{
		LOG_INFO("from %X to state %X", currentState, newState);
		currentState->event_exit.notify();
		currentState = newState;
		currentState->event_enter.notify();
	}
}

State*	StateMachine::create_state() 
{
	const auto& it = m_States.emplace(new State());
	return *it.first;
}

void	StateMachine::remove_state(State *state)
{
	delete state;
	m_States.erase(state);
}

size_t	StateMachine::size() const
{
	return m_States.size();
}

auto  StateMachine::begin() -> decltype(m_States)::iterator
{
	return m_States.begin();
}

auto StateMachine::end() -> decltype(m_States)::iterator
{
	return m_States.end();
}
