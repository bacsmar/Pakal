#pragma once
#include <Component.h>
#include <Event.h>
#include <IInputManager.h>
#include <persist/TextReader.h>

namespace Pakal
{
	class IEntity;
	class ICharacterHandler;

	class InputHandler
	{
	public:
		virtual ~InputHandler(){}	
		virtual bool has_linked_components() const = 0;		
		virtual void set_and_load_character_handler(ICharacterHandler* characterHandler) = 0;
		virtual void remove_subscriptions() = 0;
		virtual bool load_mapping(TextReader& reader, std::istream& stream) = 0;

		Event<std::string> evt_translated_command;
	};


	class KeyboardHandlerComponent : public Component, public InputHandler
	{
		DECLARE_RTTI_WITH_BASE(KeyboardHandlerComponent, Component);

	public:

		struct Settings
		{
			bool use_key_up = true;
			bool use_key_down = true;
		}settings;

		struct KeyMapping
		{
			Pakal::Key key;
			std::string command;
			enum class KeyEvent
			{
				DOWN, UP,
			};
			KeyEvent event;
		};		

		explicit KeyboardHandlerComponent(IInputManager* inputManager);
		~KeyboardHandlerComponent();		
	protected:
		bool has_linked_components() const override;	// override
		void set_and_load_character_handler(ICharacterHandler* characterHandler) override;
		void remove_subscriptions() override;
		bool load_mapping(TextReader& reader, std::istream& stream) override;
	private:
		void traslate_command_kdown(Pakal::KeyArgs args);
		void traslate_command_kUp(Pakal::KeyArgs args);		

		IInputManager*		m_input_manager_ref;
		Pakal::ulonglong	m_id_event_key_down;
		Pakal::ulonglong	m_id_event_key_up;
		bool				m_has_linked_components = false;
		ICharacterHandler*	m_handled_component;
		Component*			m_automata_Component;
		std::list<KeyMapping>	m_key_map;
	};


}
