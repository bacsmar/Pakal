#include "state.h"
#include "persist/Archive.h"
#include "Components/ScriptComponent.h"
#include "LogMgr.h"

void Pakal::TransitionCondition::persist(Archive* archive)
{
	archive->refer("target_state", m_target_state);
	archive->value("enabled", m_enabled);
	archive->value("fn_condition", fn_condition);
}

void Pakal::TransitionCondition::set_script(ScriptComponent& script)
{
	if (fn_condition.empty() == false)
	{
		m_condition = ([=, &script]() -> bool
		{
			bool retValue = false;
			const auto& result = script.call_script(fn_condition);
			if (result->is_ok())
			{
				retValue = *result;
			}
			return retValue;
		});
	}
}

void Pakal::State::persist(Archive* archive)
{
	archive->value("name", m_name);
	archive->value("on_enter", on_enter_str);
	archive->value("on_exit", on_exit_str);
	archive->value("on_update", on_update_str);

	archive->value("", "transition", m_transition_conditions);
	archive->refer("", "command", m_transition_commands, "name", "target_state");

	for (auto & s : m_transition_commands)
	{
		if (s.first.length() == 0)
		{
			LOG_ERROR("[state machine] undefined command name in transition %s", m_name.c_str());
		}
	}
}

void Pakal::State::set_script(ScriptComponent& script)
{
	if (on_enter_str.empty() == false)
	{
		event_enter = [&]()
		{
			script.call_function(on_enter_str);
		};
	}
	if (on_exit_str.empty() == false)
	{
		event_exit = ([&]()
		{
			script.call_function(on_exit_str);
		});
	}
	if (on_update_str.empty() == false)
	{
		event_update = ([&]()
		{
			script.call_function(on_update_str);
		});
	}
	for (auto& transition : m_transition_conditions)
	{
		transition.set_script(script);
	}
}