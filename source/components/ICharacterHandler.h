
#pragma once
#include <string>

namespace Pakal 
{
	class AutomataComponent;
	class KeyboardHandlerComponent;

	class ICharacterHandler
	{
	public:
		virtual ~ICharacterHandler() {}

		virtual std::string get_character_handler_typename() const = 0;
		virtual bool suscribe_to_input_handler(KeyboardHandlerComponent* inputHandler) = 0;
		virtual void unsuscribe(KeyboardHandlerComponent* inputHandler) = 0;
	};
}