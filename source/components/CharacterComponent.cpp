#include "CharacterComponent.h"

#include "StateMachine.h"
#include "persist/XmlReader.h"
#include "ResourceManager.h"
#include "InputHandlerComponent.h"

Pakal::CharacterComponent::~CharacterComponent()
{
	for (auto listenedInput = m_listened_inputsComponents.begin(); listenedInput!= m_listened_inputsComponents.end(); )
	{
		auto inputHandler = listenedInput->handler;
		inputHandler->evt_translated_command.remove_listener(listenedInput->event_id);
		m_listened_inputsComponents.erase(listenedInput++);
		inputHandler->remove_subscriptions();		
	}	
	m_listened_inputsComponents.clear();
}

std::string Pakal::CharacterComponent::get_character_handler_typename() const
{
	return m_character_handler_typename;
}

bool Pakal::CharacterComponent::suscribe_to_input_handler(KeyboardHandlerComponent* inputHandler)
{
	const auto& handler = m_listened_inputsComponents.find({ inputHandler,0 });

	ASSERT_MSG(handler == m_listened_inputsComponents.end(), "this Character handler is already sucribed to that input handler");

	bool result = false;

	if( handler == m_listened_inputsComponents.end() )
	{
		ListenedInput input;
		input.handler = inputHandler;
		input.event_id = inputHandler->evt_translated_command.add_listener([=](const std::string& command)
		{
			this->process_command(command);
		});

		result = m_listened_inputsComponents.insert(input).second;
	}	

	return result;
}

void Pakal::CharacterComponent::unsuscribe(KeyboardHandlerComponent* inputHandler)
{
	const auto& handler =  m_listened_inputsComponents.find({ inputHandler,0 });
	if( handler != m_listened_inputsComponents.end() )
	{
		inputHandler->evt_translated_command.remove_listener(handler->event_id);
		m_listened_inputsComponents.erase(handler);
		inputHandler->remove_subscriptions();
	}	
}