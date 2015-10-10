#include "AutomataComponent.h"
#include "StateMachine.h"

Pakal::AutomataComponent::~AutomataComponent()
{
	delete m_stateMachine;
}

Pakal::AutomataComponent::AutomataComponent()
{
	m_stateMachine = new StateMachine();
}

void Pakal::AutomataComponent::update()
{
	m_stateMachine->update();
}