#include "InputHandlerComponent.h"
#include <IInputManager.h>
#include <persist/TextReader.h>
#include "ICharacterHandler.h"
#include "LogMgr.h"
#include <algorithm>
#include "InputBinder.h"
#include <Pakal_II/source/persist/XmlReader.h>
#include <Pakal_II/source/ResourceManager.h>

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
	load_mapping(handledType, characterHandler->get_character_handler_fileName());

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

	m_PakalMapping.clear();
	this->loadPakalMapping();

}

Pakal::KeyboardHandlerComponent::~KeyboardHandlerComponent()
{
	m_input_manager_ref->event_key_up.remove_listener(m_id_event_key_up);
	m_input_manager_ref->event_key_down.remove_listener(m_id_event_key_down);
	m_input_manager_ref = nullptr;
	m_PakalMapping.clear();
	m_key_map.clear();
	
}



bool Pakal::KeyboardHandlerComponent::load_mapping(const std::string& mappingName, const std::string& mapping_file_name)
{

    //TODO: update to receive a IStream dont use the reader here
	//Pakal::XmlReader reader;
	//auto bindingMapStream = Pakal::ResourceManager::instance().open_read_resource(m_handled_component_File);
	//ControllerBindings::BindingMap keymapping;
	//keymapping.load(reader, *bindingMapStream);//
//	this->loadMapping(keymapping); //TODO change the list for a hashMap
//
//--- search for the name of the entity
	Pakal::XmlReader reader;
	auto bindingMapStream = Pakal::ResourceManager::instance().open_read_resource(mapping_file_name);
	
	BindingMap keyBindingMapping;
	keyBindingMapping.load(reader, *bindingMapStream);
	int keyBoardIndex = 0;
	int keyBoardCount = 0;
	m_key_map.clear();
	if (0 == keyBindingMapping.entity_name.compare(mappingName))
	{
		for (size_t i = 0; i < keyBindingMapping.m_inputDevices.size(); ++i)
		{ //--- SEARCH FOR THE first defined KEYBOARD
			if (0 == keyBindingMapping.m_inputDevices[i].device_Type.compare("KEYBOARD"))
			{
				keyBoardIndex = i;
				keyBoardCount++;
			}
		}
		//-- if two or more keyboards are defined for the same entity
		ASSERT_MSG(keyBoardCount == 1, "binding file have one of the following"
			" conditions:1.doesn't have a KEYBOARD as deviceType or the entity has multiple keyboards as deviceType");
		//--- load all the mappings found in the index
		for (size_t i = 0; i < keyBindingMapping.m_inputDevices[keyBoardIndex].get_size(); ++i)
		{
			auto& bindMap = keyBindingMapping.m_inputDevices[keyBoardIndex].get_BindMap(i);//TODO: check if memrory leak
			this->m_key_map.push_front({ t_string_to_PakalKey(bindMap.bind_Name),bindMap.command ,KeyMapping::translateEvent(bindMap.event_Type) });
		}
	}


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

Pakal::Key Pakal::KeyboardHandlerComponent::t_string_to_PakalKey(const std::string & stringKey)
{
	const auto& key = m_PakalMapping.find(stringKey);
	if (key != m_PakalMapping.end())
	{
		return key->second;
	}

	return Pakal::Key::Unknown;
}

void Pakal::KeyboardHandlerComponent::loadPakalMapping()
{
	m_PakalMapping.insert(std::make_pair("UP", Pakal::Key::Up));
	m_PakalMapping.insert(std::make_pair("DOWN", Pakal::Key::Down));
	m_PakalMapping.insert(std::make_pair("LEFT", Pakal::Key::Left));
	m_PakalMapping.insert(std::make_pair("RIGHT", Pakal::Key::Right));
	m_PakalMapping.insert(std::make_pair("A", Pakal::Key::A));
	m_PakalMapping.insert(std::make_pair("B", Pakal::Key::B));
	m_PakalMapping.insert(std::make_pair("C", Pakal::Key::C));
	m_PakalMapping.insert(std::make_pair("D", Pakal::Key::D));
	m_PakalMapping.insert(std::make_pair("E", Pakal::Key::E));
	m_PakalMapping.insert(std::make_pair("F", Pakal::Key::F));
	m_PakalMapping.insert(std::make_pair("G", Pakal::Key::G));
	m_PakalMapping.insert(std::make_pair("H", Pakal::Key::H));
	m_PakalMapping.insert(std::make_pair("I", Pakal::Key::I));
	m_PakalMapping.insert(std::make_pair("J", Pakal::Key::J));
	m_PakalMapping.insert(std::make_pair("K", Pakal::Key::K));
	m_PakalMapping.insert(std::make_pair("L", Pakal::Key::L));
	m_PakalMapping.insert(std::make_pair("M", Pakal::Key::M));
	m_PakalMapping.insert(std::make_pair("Ñ", Pakal::Key::N));
	m_PakalMapping.insert(std::make_pair("O", Pakal::Key::O));
	m_PakalMapping.insert(std::make_pair("P", Pakal::Key::P));
	m_PakalMapping.insert(std::make_pair("Q", Pakal::Key::Q));
	m_PakalMapping.insert(std::make_pair("R", Pakal::Key::R));
	m_PakalMapping.insert(std::make_pair("S", Pakal::Key::S));
	m_PakalMapping.insert(std::make_pair("T", Pakal::Key::T));
	m_PakalMapping.insert(std::make_pair("U", Pakal::Key::U));
	m_PakalMapping.insert(std::make_pair("V", Pakal::Key::V));
	m_PakalMapping.insert(std::make_pair("X", Pakal::Key::X));
	m_PakalMapping.insert(std::make_pair("Y", Pakal::Key::Y));
	m_PakalMapping.insert(std::make_pair("Z", Pakal::Key::Z));

	m_PakalMapping.insert(std::make_pair("NUM1", Pakal::Key::Num1));
	m_PakalMapping.insert(std::make_pair("NUM2", Pakal::Key::Num2));
	m_PakalMapping.insert(std::make_pair("NUM3", Pakal::Key::Num3));
	m_PakalMapping.insert(std::make_pair("NUM4", Pakal::Key::Num4));
	m_PakalMapping.insert(std::make_pair("NUM5", Pakal::Key::Num5));
	m_PakalMapping.insert(std::make_pair("NUM6", Pakal::Key::Num6));
	m_PakalMapping.insert(std::make_pair("NUM7", Pakal::Key::Num7));
	m_PakalMapping.insert(std::make_pair("NUM8", Pakal::Key::Num8));
	m_PakalMapping.insert(std::make_pair("NUM9", Pakal::Key::Num9));
	m_PakalMapping.insert(std::make_pair("NUM0", Pakal::Key::Num0));

	m_PakalMapping.insert(std::make_pair("NUMPAD1", Pakal::Key::Numpad1));
	m_PakalMapping.insert(std::make_pair("NUMPAD2", Pakal::Key::Numpad2));
	m_PakalMapping.insert(std::make_pair("NUMPAD3", Pakal::Key::Numpad3));
	m_PakalMapping.insert(std::make_pair("NUMPAD4", Pakal::Key::Numpad4));
	m_PakalMapping.insert(std::make_pair("NUMPAD5", Pakal::Key::Numpad5));
	m_PakalMapping.insert(std::make_pair("NUMPAD6", Pakal::Key::Numpad6));
	m_PakalMapping.insert(std::make_pair("NUMPAD7", Pakal::Key::Numpad7));
	m_PakalMapping.insert(std::make_pair("NUMPAD8", Pakal::Key::Numpad8));
	m_PakalMapping.insert(std::make_pair("NUMPAD9", Pakal::Key::Numpad9));
	m_PakalMapping.insert(std::make_pair("NUMPAD0", Pakal::Key::Numpad0));

	m_PakalMapping.insert(std::make_pair("SPACE", Pakal::Key::Space));
	m_PakalMapping.insert(std::make_pair("ADD", Pakal::Key::Add));
	m_PakalMapping.insert(std::make_pair("SUBTRACT", Pakal::Key::Subtract));

}
