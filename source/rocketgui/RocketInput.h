
#pragma once

#include "Config.h"

#include <Rocket/Core/Input.h>
#include <Rocket/Core/Types.h>
#include "EventArgs.h"

namespace Rocket {
namespace Core {

class Context;

}
}

namespace Pakal 
{
	
	class _PAKALExport RocketInput 
	{			
	public:
	/// Sets the context to send input events to.
	/// @param[in] context The context to send input events to.
	static void set_context(Rocket::Core::Context* context);
	/// Returns the character code for a key identifer / key modifier combination.
	/// @param[in] key_identifier The key to generate a character code for.
	/// @param[in] key_modifier_state The configuration of the key modifiers.
	/// @return The character code.
	static Rocket::Core::word GetCharacterCode(Rocket::Core::Input::KeyIdentifier key_identifier, int key_modifier_state);

	static void process_mouse_move(Pakal::MouseArgs args);
	static void process_mouse_pressed(Pakal::MouseArgs args);
	static void process_mouse_released(Pakal::MouseArgs args);

	protected:
		static Rocket::Core::Context* context;
	};
}
