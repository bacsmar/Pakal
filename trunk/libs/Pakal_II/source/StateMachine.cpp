#include "StateMachine.h"
#include "State.h"
#include "LogMgr.h"
#include "persist/Archive.h"
#include <algorithm>

using namespace Pakal;

StateMachine::StateMachine()
{	
}

StateMachine::~StateMachine(void)
{
	for(auto &it : m_states)
	{
		delete it;
	}
	m_states.clear();
}

State* StateMachine::get_state(const std::string& name) const
{
	auto found_state = std::find_if(m_states.begin(), m_states.end(), [=](State* state) { return name == state->get_name(); });	
	ASSERT_MSG( *found_state != nullptr, "can't found state %s", name.c_str());
	return *found_state;
}

void StateMachine::set_current_state(const std::string& name)
{
	auto new_state = std::find_if(m_states.begin(), m_states.end(), [=](State* state) { return name == state->get_name(); });
	if(new_state != m_states.end() )
	{
		set_current_state(*new_state);
	}else
	{
		LOG_ERROR("state %s doesnt exists", name.c_str() );
	}
}

void StateMachine::update()
{	
	ASSERT(m_current_state);	// did you forget to set set_current_state(initialState)?
	State *newState = m_current_state->exec_conditions();
	ASSERT(newState);

	if (nullptr == newState)
		return;

	if( newState != m_current_state)
	{
		m_current_state->event_exit();
		m_current_state = newState;
		m_current_state->event_enter();
	}
	else
	{
		m_current_state->event_update();
	}		
}

void StateMachine::process_command(const std::string& command)
{
	ASSERT(m_current_state);	// did you forget to set set_current_state(initialState)?
	State *newState = m_current_state->process_command(command);
	ASSERT(newState);
	if (newState != m_current_state)
	{
		m_current_state->event_exit();
		m_current_state = newState;
		m_current_state->event_enter();
	}
}

void StateMachine::set_script(ScriptComponent& script)
{
		for(auto state : m_states)
		{
			state->set_script(script);
		}
}

void StateMachine::persist(Archive* archive)
{
	archive->value("commands","command", m_available_commands);
	archive->value("","state", m_states);
	archive->refer("current_state", m_current_state);
	archive->value("script_file", m_script_file);
}

State* StateMachine::create_state(const std::string& name)
{
	const auto& it = m_states.emplace( new State(name) );
	return *it.first;	
}

void StateMachine::remove_state(State *state)
{
	delete state;
	m_states.erase(state);
}

size_t StateMachine::size() const
{
	return m_states.size();
}

auto StateMachine::begin() -> decltype(m_states)::iterator
{
	return m_states.begin();
}

auto StateMachine::end() -> decltype(m_states)::iterator
{
	return m_states.end();
}
