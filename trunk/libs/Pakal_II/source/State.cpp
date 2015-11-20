#include "state.h"
#include "persist/Archive.h"
#include "Components/ScriptComponent.h"

void Pakal::Transition::persist(Archive* archive)
{
	archive->refer("target_state", m_target_state);
	archive->value("enabled", m_enabled);
	archive->value("fn_condition", fn_condition);
}

void Pakal::Transition::set_script(ScriptComponent& script)
{
		//if (fn_condition.empty() == false)
		{
			m_condition = ([=,&script]() -> bool
				{
					bool retValue = false;
					const auto& result = script.call_script(fn_condition);
					if ( result.is_ok() )
					{
						retValue = result;
					}
					return retValue;
				});
		}
}

void Pakal::State::persist(Archive* archive)
{
	auto count = m_transitions.size();
	archive->value("name", m_name);
	archive->value("count", count);
	archive->value("", "transition", m_transitions);
	archive->value("on_enter", on_enter_str);
	archive->value("on_exit", on_exit_str);	
}

void Pakal::State::set_script(ScriptComponent& script)
{
	if (on_enter_str.empty() == false)
	{
		event_enter.add_listener([&]()
			{
				script.call_function(on_enter_str);
			});
	}
	if( on_exit_str.empty() == false)
	{
		event_exit.add_listener([&]()
			{
				script.call_function(on_exit_str);
			});
	}
	for( auto& transition : m_transitions)
	{
		transition.set_script(script);
	}
}
