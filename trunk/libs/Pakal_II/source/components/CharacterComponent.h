#pragma once

#include <string>
#include "AutomataComponent.h"
#include "ICharacterHandler.h"

namespace Pakal
{
	class InputHandler;
	class StateMachine;	
	class ScriptComponent;
	class CharacterComponent : public AutomataComponent, public ICharacterHandler
	{
	
		DECLARE_RTTI_WITH_BASE(CharacterComponent, AutomataComponent);
	public:
		~CharacterComponent() override;	
	protected:
		// Character handler stuff, access from InputComponent only
		std::string get_character_handler_typename() const override;
		std::string get_character_handler_fileName() const override;
		bool suscribe_to_input_handler(InputHandler* inputHandler) override;
		void unsuscribe(InputHandler* inputHandler) override;		

		inline void set_character_handler_typename(const std::string& type)
		{
			m_character_handler_typename = type;
		};
		inline void set_character_handler_fileName(const std::string& type)
		{
			m_character_handler_file_name = type;
		};

	private:		

		struct ListenedInput
		{
			InputHandler* handler;
			unsigned event_id; 
			friend bool operator <(const ListenedInput& lhs, const ListenedInput& rhs)
			{
				return lhs.handler < rhs.handler;
			}
		};
		std::set<ListenedInput> m_listened_inputsComponents;
		std::string m_character_handler_typename = "Biped";
		std::string m_character_handler_file_name="Biped";
	};
}
