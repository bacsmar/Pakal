#include "LuaProxies.h"

#if PAKAL_USE_SCRIPTS == 1
/////////////////////////////////////////////////////////////
OOLUA_EXPORT_FUNCTIONS(SpriteComponent, set_animation, set_flipped)
OOLUA_EXPORT_FUNCTIONS_CONST(SpriteComponent,is_playing)
/////////////////////////////////////////////////////////////
OOLUA_EXPORT_FUNCTIONS(SFXComponent, add, play)
EXPORT_OOLUA_FUNCTIONS_0_CONST(SFXComponent)
/////////////////////////////////////////////////////////////
OOLUA_EXPORT_FUNCTIONS(GamepadComponent, is_button_pressed)
//OOLUA_EXPORT_FUNCTIONS_CONST(GamepadComponent)
//OOLUA_EXPORT_FUNCTIONS(Enums, set_enum, get_enum)
EXPORT_OOLUA_FUNCTIONS_0_CONST(GamepadComponent)
/////////////////////////////////////////////////////////////
OOLUA_EXPORT_FUNCTIONS(vector2df)
OOLUA_EXPORT_FUNCTIONS_CONST(vector2df, get_x, get_y)
/////////////////////////////////////////////////////////////
OOLUA_EXPORT_FUNCTIONS(SimpleTimer, set_interval)
OOLUA_EXPORT_FUNCTIONS_CONST(SimpleTimer, expired)
/////////////////////////////////////////////////////////////
OOLUA_EXPORT_FUNCTIONS(Pakal::SpritebodyComponent, apply_impulse, set_lineal_velocity, apply_force)
OOLUA_EXPORT_FUNCTIONS_CONST(Pakal::SpritebodyComponent, get_lineal_velocity)
/////////////////////////////////////////////////////////////
//	LUA, definicion
OOLUA_EXPORT_FUNCTIONS(Pakal::LogMgr, log)
EXPORT_OOLUA_FUNCTIONS_0_CONST(Pakal::LogMgr)

OOLUA_EXPORT_FUNCTIONS(Path)
EXPORT_OOLUA_FUNCTIONS_0_CONST(Path)

OOLUA_EXPORT_FUNCTIONS(UIManager, load_document, unload_document, show_document, hide_document)
EXPORT_OOLUA_FUNCTIONS_0_CONST(UIManager)

namespace Pakal
{
	template <>
	void oolua_script_interface_for<SpriteComponent>(SpriteComponent* obj, OOLUA::Script& script, const std::string& name)
	{
		ASSERT(name.size() > 0);
		script.register_class<SpriteComponent>();
		
		auto result = OOLUA::push(script.state(), obj, OOLUA::Cpp);

		if( result)
			lua_setglobal(script.state(), name.c_str());
	}

	template <>
	void oolua_script_interface_for<SpritebodyComponent>(SpritebodyComponent* obj, OOLUA::Script& script, const std::string& name)
	{
		ASSERT(name.size() > 0);
		script.register_class<SpritebodyComponent>();

		auto result = OOLUA::push(script.state(), obj, OOLUA::Cpp);

		if (result)
			lua_setglobal(script.state(), name.c_str());
	}

	template <>
	void oolua_script_interface_for<GamepadComponent>(GamepadComponent* obj, OOLUA::Script& script, const std::string& name)
	{
		ASSERT(name.size() > 0);
		script.register_class<GamepadComponent>();

		auto result = OOLUA::push(script.state(), obj, OOLUA::Cpp);

		if (result)
			lua_setglobal(script.state(), name.c_str());
	}

	template <>
	void oolua_script_interface_for<SFXComponent>(SFXComponent* obj, OOLUA::Script& script, const std::string& name)
	{
		ASSERT(name.size() > 0);
		script.register_class<SFXComponent>();

		auto result = OOLUA::push(script.state(), obj, OOLUA::Cpp);

		if (result)
			lua_setglobal(script.state(), name.c_str());
	}

}

#if PAKAL_USE_SELENE_LUA

#pragma warning (disable : 4800 )
#include "selene.h"

namespace Pakal
{
	template <>
	void selene_script_interface_for<SpriteComponent>(Pakal::SpriteComponent* obj, sel::State& state, const std::string& name)
	{
		state[name.c_str()].SetObj(*obj, 
			"set_animation", &SpriteComponent::set_animation,
			"set_flipped",&SpriteComponent::set_flipped,
			"is_playing",&SpriteComponent::is_playing
			);
		state.HandleExceptionsPrintingToStdOut();
		//state["sprite"]["set_animation"](std::string( "test"));
		state("function on_enter() sprite:set_animation(\"test\"); end");
		state["on_enter"]();		
	}

	//template <>
	//void selene_script_interface_for<GamepadComponent>(GamepadComponent* obj, sel::State& state)
	//{
	//	state["sprite"].SetObj(*obj,
	//		"is_button_pressed", &GamepadComponent::is_button_pressed			
	//		);
	//	//state["sprite"]["is_playing"]();
	//}

	template <>
	void selene_script_interface_for<SpritebodyComponent>(SpritebodyComponent* obj, sel::State& state, const std::string& name)
	{
		state["vector2df"].SetClass<Pakal::tmath::vector2df>();
		state[name.c_str()].SetObj(*obj,
			"apply_impulse", &SpritebodyComponent::apply_impulse,
			"set_lineal_velocity", &SpritebodyComponent::set_lineal_velocity,
			"get_lineal_velocity", &SpritebodyComponent::get_lineal_velocity
			);
		//state.Load()
		//state["body"]["apply_impulse"](tmath::vector2df( { 0,0 }));
	} 

}
#endif

#endif	//PAKAL_USE_SCRIPTS == 1