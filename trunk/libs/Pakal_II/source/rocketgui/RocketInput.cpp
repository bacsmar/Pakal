#include "RocketInput.h"
#include <Rocket/Core/Context.h>
#include <Rocket/Core/Input.h>


Rocket::Core::Context* Pakal::RocketInput::context = 0;

void Pakal::RocketInput::set_context(Rocket::Core::Context* context_)
{
	context = context_;
}

Rocket::Core::word Pakal::RocketInput::GetCharacterCode(Rocket::Core::Input::KeyIdentifier key_identifier, int key_modifier_state)
{
	return Rocket::Core::word();
}

void Pakal::RocketInput::process_mouse_pressed(Pakal::MouseArgs args)
{
	context->ProcessMouseButtonDown(args.button_id, 0);
}
void Pakal::RocketInput::process_mouse_released(Pakal::MouseArgs args)
{
	context->ProcessMouseButtonUp(args.button_id, 0);
}

void Pakal::RocketInput::process_mouse_move(Pakal::MouseArgs  args)
{
	context->ProcessMouseMove(args.x, args.y, 0);
}
