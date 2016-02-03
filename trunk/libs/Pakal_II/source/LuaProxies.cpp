#include "LuaProxies.h"
#include "components/ScriptComponent_Lua.h"
#include "components/SpriteComponent.h"
#include "components/SpritePhysicsComponent.h"
#include "components/SFXComponent.h"
#include "components/GamepadComponent.h"
#include "IUIManager.h"
#include "SimpleTimer.h"
#include "LogMgr.h"

#if PAKAL_USE_SCRIPTS == 1

//OOLUA_EXPORT_FUNCTIONS(UIManager, load_document, unload_document, show_document, hide_document)
//EXPORT_OOLUA_FUNCTIONS_0_CONST(UIManager)

namespace Pakal 
{

	template <class T, class TReturn, class ...Args>
	auto fn_type_cast(TReturn(T::* c)(Args...args)) -> decltype (c)
	{
		return c;
	}

	template <>
	void register_script_interface_for<SpriteComponent>(SpriteComponent* obj, Script& script, const std::string& name)
	{				
		script.register_class<SpriteComponent>(name)
			.add_function("set_animation", &SpriteComponent::set_animation)
			.add_function("set_flipped", &SpriteComponent::set_flipped)
			.add_function("is_playing", &SpriteComponent::is_playing)
			.add_function("is_flipped", &SpriteComponent::is_flipped)
			.add_function("is_flipped", &SpriteComponent::is_flipped)
			.add_function("get_position", &SpriteComponent::get_position);

		script.register_obj(name, obj);
	}
	template <>
	void register_script_interface_for<SpritePhysicsComponent>(SpritePhysicsComponent* obj, Script& script, const std::string& name)
	{
		script.register_class<SpritePhysicsComponent>(name)
			.add_function("apply_force", fn_type_cast<SpritePhysicsComponent, void, float, float>(&SpritePhysicsComponent::apply_force))
			.add_function("apply_impulse", fn_type_cast<SpritePhysicsComponent, void, float, float>(&SpritePhysicsComponent::apply_impulse))
			.add_function("set_lineal_velocity", fn_type_cast<SpritePhysicsComponent, void, float, float>(&SpritePhysicsComponent::set_lineal_velocity))
			.add_function("get_angle", &SpritePhysicsComponent::get_angle)
			.add_function("get_lineal_velocity", &SpritePhysicsComponent::get_lineal_velocity)
			.add_function("get_scale", &SpritePhysicsComponent::get_scale)
			.add_function("get_position", &SpritePhysicsComponent::get_position);

		script.register_obj(name, obj);
	}
	template <>
	void register_script_interface_for<GamepadComponent>(GamepadComponent* obj, Script& script, const std::string& name)
	{
		script.register_class<GamepadComponent>(name)
			.add_function("is_button_pressed", &GamepadComponent::is_button_pressed)
			.add_constant("Button_RIGHT", GamepadComponent::Button_RIGHT)
			.add_constant("Button_LEFT", GamepadComponent::Button_LEFT)
			.add_constant("Button_UP", GamepadComponent::Button_UP)
			.add_constant("Button_DOWN", GamepadComponent::Button_DOWN)
			.add_constant("Button_A", GamepadComponent::Button_A)
			.add_constant("Button_X", GamepadComponent::Button_X)
			.add_constant("Button_Y", GamepadComponent::Button_Y)
			.add_constant("Button_B", GamepadComponent::Button_B)
			;
		
		script.register_obj(name, obj);
	}	

	template <>
	void register_script_interface_for<SFXComponent>(SFXComponent* obj, Script& script, const std::string& name)
	{
		//using addType = bool (SFXComponent::* )(unsigned, const std::string&);		
		//bool (SFXComponent::* d)(unsigned, const std::string&) = &SFXComponent::add;

		//auto ac = fn_type_cast<SFXComponent, bool,unsigned, const std::string&>(&SFXComponent::add);

		//script.register_class(name, obj, 
		//script.register_obj(name, obj,
		script.register_class<SFXComponent>(name)
			.add_function("add", fn_type_cast<SFXComponent, bool, unsigned, const std::string&>(&SFXComponent::add))
			.add_function("play", fn_type_cast<SFXComponent, void, unsigned>(&SFXComponent::play));
		script.register_obj(name, obj);
	}
}

#endif	//PAKAL_USE_SCRIPTS == 1