#pragma once

#include "Component.h"
#include <string>
#include "StateMachine.h"

namespace Pakal
{	
	class StateMachine;	
	class ScriptComponent;
	class AutomataComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(AutomataComponent, Component);
	public:
		~AutomataComponent() override;
		AutomataComponent();

		void update() const;

		void process_command(const std::string command) const
		{
			ASSERT(m_stateMachine);
			m_stateMachine->process_command(command);			
		}

		void set_script(ScriptComponent& script) const
		{
			m_stateMachine->set_script(script);
		}


		/*inline StateMachine* get_state_machine() const
		{
			return m_stateMachine;
		}*/

		inline const std::vector<std::string>& get_available_commands() const
		{
			return m_stateMachine->get_available_commands();
		}

		bool load_from_file(const std::string& file) const;

	private:
		StateMachine *m_stateMachine;
	};
}