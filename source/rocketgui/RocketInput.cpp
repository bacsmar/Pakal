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
	context->ProcessMouseButtonDown(static_cast<int>(args.button_id), 0);	
}
void Pakal::RocketInput::process_mouse_released(Pakal::MouseArgs args)
{
	context->ProcessMouseButtonUp(static_cast<int>(args.button_id), 0);
}

void Pakal::RocketInput::process_mouse_move(Pakal::MouseArgs  args)
{
	context->ProcessMouseMove(args.x, args.y, 0);
}

void Pakal::RocketInput::process_text_input(Pakal::TextArgs args)
{
	context->ProcessTextInput(args.unicode);
}

void Pakal::RocketInput::process_key_down(Pakal::KeyArgs args)
{
	context->ProcessKeyDown(pakal_to_rocket(args.key), args.key_modifier);
}

void Pakal::RocketInput::process_key_up(Pakal::KeyArgs args)
{
	context->ProcessKeyUp(pakal_to_rocket(args.key), args.key_modifier);
}
using namespace Rocket::Core::Input;
Rocket::Core::Input::KeyIdentifier Pakal::RocketInput::pakal_to_rocket(Pakal::Key key)
{
	switch (key)
	{
	case Key::A : return KeyIdentifier::KI_A; break;
	case Key::B : return KeyIdentifier::KI_B; break;
	case Key::C : return KeyIdentifier::KI_C; break;
	case Key::D : return KeyIdentifier::KI_D; break;
	case Key::E : return KeyIdentifier::KI_E; break;
	case Key::F : return KeyIdentifier::KI_F; break;
	case Key::G : return KeyIdentifier::KI_G; break;
	case Key::H : return KeyIdentifier::KI_H; break;
	case Key::I : return KeyIdentifier::KI_I; break;
	case Key::J : return KeyIdentifier::KI_J; break;
	case Key::K : return KeyIdentifier::KI_K; break;
	case Key::L : return KeyIdentifier::KI_L; break;
	case Key::M : return KeyIdentifier::KI_M; break;
	case Key::N : return KeyIdentifier::KI_N; break;
	case Key::O : return KeyIdentifier::KI_O; break;
	case Key::P : return KeyIdentifier::KI_P; break;
	case Key::Q : return KeyIdentifier::KI_Q; break;
	case Key::R : return KeyIdentifier::KI_R; break;
	case Key::S : return KeyIdentifier::KI_S; break;
	case Key::T : return KeyIdentifier::KI_T; break;
	case Key::U : return KeyIdentifier::KI_U; break;
	case Key::V : return KeyIdentifier::KI_V; break;
	case Key::W : return KeyIdentifier::KI_W; break;
	case Key::X : return KeyIdentifier::KI_X; break;
	case Key::Y : return KeyIdentifier::KI_Y; break;
	case Key::Z : return KeyIdentifier::KI_Z; break;
	case Key::Unknown: break;
	case Key::Num0: return KI_0; break;
	case Key::Num1: return KI_1; break;
	case Key::Num2: return KI_2; break;
	case Key::Num3: return KI_3; break;
	case Key::Num4: return KI_4; break;
	case Key::Num5: return KI_5; break;
	case Key::Num6: return KI_6; break;
	case Key::Num7: return KI_7; break;
	case Key::Num8: return KI_8; break;
	case Key::Num9: return KI_9; break;
	case Key::Escape: return KI_ESCAPE; break;
	case Key::LControl: break;
	case Key::LShift: break;
	case Key::LAlt: break;
	case Key::LSystem: break;
	case Key::RControl: break;
	case Key::RShift: break;
	case Key::RAlt: break;
	case Key::RSystem: break;
	case Key::Menu: break;
	case Key::LBracket: break;
	case Key::RBracket: break;
	case Key::SemiColon: break;
	case Key::Comma: return KI_OEM_COMMA; break;
	case Key::Period: return KI_OEM_PERIOD; break;
	case Key::Quote: break;
	case Key::Slash: break;
	case Key::BackSlash: break;
	case Key::Tilde: break;
	case Key::Equal: break;
	case Key::Dash: break;
	case Key::Space: return KI_SPACE; break;
	case Key::Return: break;
	case Key::BackSpace: return KI_BACK; break;
	case Key::Tab: return KI_TAB; break;
	case Key::PageUp: return KI_PRIOR; break;
	case Key::PageDown: return KI_NEXT; break;
	case Key::End: return KI_END; break;
	case Key::Home: return KI_HOME; break;
	case Key::Insert: return KI_INSERT; break;
	case Key::Delete: return KI_DELETE; break;
	case Key::Add: return KI_ADD; break;
	case Key::Subtract: return KI_SUBTRACT; break;
	case Key::Multiply: return KI_MULTIPLY; break;
	case Key::Divide: return KI_DIVIDE; break;
	case Key::Left: return KI_LEFT; break;
	case Key::Right: return KI_RIGHT; break;
	case Key::Up: return KI_UP; break;
	case Key::Down: return KI_DOWN; break;
	case Key::Numpad0:	return KI_NUMPAD0; break;
	case Key::Numpad1: return KI_NUMPAD1; break;
	case Key::Numpad2: return KI_NUMPAD2; break;
	case Key::Numpad3: return KI_NUMPAD3; break;
	case Key::Numpad4: return KI_NUMPAD4; break;
	case Key::Numpad5: return KI_NUMPAD5; break;
	case Key::Numpad6: return KI_NUMPAD6; break;
	case Key::Numpad7: return KI_NUMPAD7; break;
	case Key::Numpad8: return KI_NUMPAD8; break;
	case Key::Numpad9: return KI_NUMPAD9; break;
	case Key::F1: return KI_F1; break;
	case Key::F2: return KI_F2; break;
	case Key::F3: return KI_F3; break;
	case Key::F4: return KI_F4; break;
	case Key::F5: return KI_F5; break;
	case Key::F6: return KI_F6; break;
	case Key::F7: return KI_F7; break;
	case Key::F8: return KI_F8; break;
	case Key::F9: return KI_F9; break;
	case Key::F10: return KI_F10; break;
	case Key::F11: return KI_F11; break;
	case Key::F12: return KI_F12; break;
	case Key::F13: return KI_F13; break;
	case Key::F14: return KI_F14; break;
	case Key::F15: return KI_F15; break;
	case Key::Pause: break;
	case Key::KeyCount: break;
	default: break;
	}
	return KI_UNKNOWN;
}
