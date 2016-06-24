#pragma once
#include <Component.h>
#include <Event.h>
#include <IInputManager.h>
#include <persist/TextReader.h>

namespace Pakal
{
	class Entity;
	class ICharacterHandler;

	class _PAKALExport InputHandler
	{
	public:
		virtual ~InputHandler();

		bool has_linked_components() const;;
		void set_handled_character(ICharacterHandler* characterHandler);;
		void remove_subscriptions();
		virtual bool load_mapping(const std::string& mappingName, const std::string& mappingFileName) = 0;

		Event<std::string> evt_translated_command;
	private:
		ICharacterHandler*	m_handled_component = nullptr;
		bool				m_has_linked_components = false;
	};


	class _PAKALExport KeyboardHandlerComponent : public Component, public InputHandler
	{
		DECLARE_RTTI_WITH_BASE(KeyboardHandlerComponent, Component);

	public:
		struct KeyMapping
		{
			Pakal::Key key;
			std::string command;
			enum class Type
			{
				DOWN, UP,
			};
			Type event_type;
			static Type translateEvent(const std::string& stringValue)
			{
				if (stringValue.compare("DOWN") == 0) return Type::DOWN;
				if (0 == stringValue.compare("UP"))return Type::UP;
				return Type::UP;
			}
		};		

		explicit KeyboardHandlerComponent(IInputManager* inputManager);
		~KeyboardHandlerComponent();
	protected:		
		bool load_mapping(const std::string& mappingName, const std::string& mappingFileName) override;
	private:
		void traslate_command_kdown(Pakal::KeyArgs args);
		void traslate_command_kUp(Pakal::KeyArgs args);
		Pakal::Key t_string_to_PakalKey(const std::string&);
		void loadPakalMapping();

		IInputManager*		m_input_manager_ref;
		Pakal::EventId	m_id_event_key_down;
		Pakal::EventId	m_id_event_key_up;		
		std::list<KeyMapping>	m_key_map;
		std::map<std::string, Pakal::Key> m_PakalMapping;
	};


}
