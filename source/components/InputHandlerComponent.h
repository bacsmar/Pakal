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
		virtual bool load_mapping(const std::string& mappingName) = 0;

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
		};		

		explicit KeyboardHandlerComponent(IInputManager* inputManager);
		~KeyboardHandlerComponent();		
	protected:		
		bool load_mapping(const std::string& mappingName) override;
	private:
		void traslate_command_kdown(Pakal::KeyArgs args);
		void traslate_command_kUp(Pakal::KeyArgs args);		

		IInputManager*		m_input_manager_ref;
		Pakal::EventId	m_id_event_key_down;
		Pakal::EventId	m_id_event_key_up;		
		std::list<KeyMapping>	m_key_map;
	};


}
