
#pragma once
#include <string>

namespace Pakal 
{
	class InputHandler;

	class ICharacterHandler
	{
	public:
		virtual ~ICharacterHandler() {}

		virtual std::string get_character_handler_typename() const = 0;
		virtual bool suscribe_to_input_handler(InputHandler* inputHandler) = 0;
		virtual void unsuscribe(InputHandler* inputHandler) = 0;
	};
}