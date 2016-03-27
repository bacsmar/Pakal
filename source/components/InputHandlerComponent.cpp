#include "InputHandlerComponent.h"
#include <IInputManager.h>
#include <persist/TextReader.h>
#include "ICharacterHandler.h"
#include "LogMgr.h"
#include <algorithm>


Pakal::InputHandler::~InputHandler()
{
	if (true == m_has_linked_components)
	{
		m_handled_component->unsuscribe(this);
	}
	m_handled_component = nullptr;
}

bool Pakal::InputHandler::has_linked_components() const
{
	return m_has_linked_components;
}

void Pakal::InputHandler::set_handled_character(ICharacterHandler* characterHandler)
{
	ASSERT_MSG(m_has_linked_components == false, "input handler already links a CharacterDriver, to make a link again please use remove_subscriptions() ");

	auto handledType = characterHandler->get_character_handler_typename();
	LOG_INFO("[input handler] loading mapping for: %s", handledType.c_str());

	this->m_handled_component = characterHandler;

	//load mapping
	load_mapping(handledType);

	m_has_linked_components = m_handled_component->suscribe_to_input_handler(this);
}

void Pakal::InputHandler::remove_subscriptions()
{
	ASSERT(m_handled_component);
	m_handled_component->unsuscribe(this);//--if the caller was the handled itself.. it not longer unsubscribes
	m_handled_component = nullptr;	
	m_has_linked_components = false;
}

Pakal::KeyboardHandlerComponent::KeyboardHandlerComponent(Pakal::IInputManager* inputManager) : m_input_manager_ref(inputManager)
{
	m_id_event_key_down = m_input_manager_ref->event_key_down.add_listener([=](const Pakal::KeyArgs& args)
	{
		this->traslate_command_kdown(args);
	}, THIS_THREAD);
	m_id_event_key_up = m_input_manager_ref->event_key_up.add_listener([=](const Pakal::KeyArgs& args)
	{
		this->traslate_command_kUp(args);
	}, THIS_THREAD);

}

Pakal::KeyboardHandlerComponent::~KeyboardHandlerComponent()
{
	m_input_manager_ref->event_key_up.remove_listener(m_id_event_key_up);
	m_input_manager_ref->event_key_down.remove_listener(m_id_event_key_down);
	m_input_manager_ref = nullptr;
	
}

bool Pakal::KeyboardHandlerComponent::load_mapping(const std::string& mappingName)
{

	m_key_map.clear();
	this->m_key_map.push_front({ Pakal::Key::Left,"WALK_LEFT" , KeyMapping::Type::DOWN });
	this->m_key_map.push_front({ Pakal::Key::Right,"WALK_RIGHT", KeyMapping::Type::DOWN });
	this->m_key_map.push_front({ Pakal::Key::Down,"CROUCH", KeyMapping::Type::DOWN });
	this->m_key_map.push_front({ Pakal::Key::Up,"JUMP", KeyMapping::Type::DOWN });

	this->m_key_map.push_front({ Pakal::Key::A,"FIERCE_PUNCH", KeyMapping::Type::DOWN });
	this->m_key_map.push_front({ Pakal::Key::S,"FIERCE_KICK", KeyMapping::Type::DOWN });
	this->m_key_map.push_front({ Pakal::Key::Z,"BLOCK", KeyMapping::Type::DOWN });

	this->m_key_map.push_front({ Pakal::Key::Left,"STOP_WL" , KeyMapping::Type::UP });
	this->m_key_map.push_front({ Pakal::Key::Right,"STOP_WR", KeyMapping::Type::UP });
	this->m_key_map.push_front({ Pakal::Key::Down,"STOP_CROUCH", KeyMapping::Type::UP });
	this->m_key_map.push_front({ Pakal::Key::Z,"STOP_BLOCK", KeyMapping::Type::UP });

	LOG_WARNING("this function is not fully implemented");

	return true;

}

void Pakal::KeyboardHandlerComponent::traslate_command_kdown(Pakal::KeyArgs args)
{
	auto result = std::find_if(m_key_map.begin(), m_key_map.end(), [=](const KeyMapping& mapping) -> bool 
	{
		return mapping.key == args.key && mapping.event_type == KeyMapping::Type::DOWN;
	});

	if (result != m_key_map.end())
		evt_translated_command.notify(result->command);
}

void Pakal::KeyboardHandlerComponent::traslate_command_kUp(Pakal::KeyArgs args)
{
	auto result = std::find_if(m_key_map.begin(), m_key_map.end(), [=](const KeyMapping& mapping) -> bool
	{
		return mapping.key == args.key && mapping.event_type == KeyMapping::Type::UP;
	});

	if (result != m_key_map.end())
		evt_translated_command.notify(result->command);
}