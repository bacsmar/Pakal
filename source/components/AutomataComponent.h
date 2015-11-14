#pragma once

#include "Component.h"

namespace Pakal
{	
	class StateMachine;
	class AutomataComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(AutomataComponent, Component);
	public:
		~AutomataComponent() override;
		AutomataComponent();

		void update();

		inline StateMachine* get_states() const
		{
			return m_stateMachine;
		}

	private:
		StateMachine *m_stateMachine;	
		
	};
}