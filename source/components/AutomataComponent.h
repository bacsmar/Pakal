#pragma once

#include "Component.h"
#include <string>
#include "StateMachine.h"

namespace Pakal
{	
	class StateMachine;	
	class AutomataComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(AutomataComponent, Component);
	public:
		~AutomataComponent() override;
		AutomataComponent();

		void update() const;

		inline StateMachine* get_state_machine() const
		{
			return m_stateMachine;
		}

		bool load_from_file(const std::string& file) const;

	private:
		StateMachine *m_stateMachine;
	};
}