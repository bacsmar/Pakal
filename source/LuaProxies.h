#pragma once

#include "Config.h"
#include "oolua.h"

#include "components/SpriteComponent.h"
#include "components/GamepadComponent.h"
#include "components/SpritebodyComponent.h"
#include "components/SFXComponent.h"
#include "components/ScriptComponent.h"

namespace OOLUA
{
	//typedef TEST_NAMESPACED_CLASS::Namespaced Namespaced;
	using SpriteComponent = Pakal::SpriteComponent;
	using GamepadComponent = Pakal::GamepadComponent;
	using SpritebodyComponent = Pakal::SpritebodyComponent;
	using vector2df = Pakal::tmath::vector2df;
	using SFXComponent = Pakal::SFXComponent;
} // namespace OOLUA

#if PAKAL_USE_SCRIPTS == 1
#include "LogMgr.h"

OOLUA_PROXY(SpriteComponent)//class has no bases
OOLUA_TAGS(OOLUA::No_public_constructors, OOLUA::Abstract)
OOLUA_MFUNC(set_animation)
OOLUA_MFUNC(set_flipped)
OOLUA_MFUNC_CONST(is_playing)
OOLUA_PROXY_END

OOLUA_PROXY(SFXComponent)//class has no bases
OOLUA_TAGS(OOLUA::No_public_constructors, OOLUA::Abstract)
OOLUA_MEM_FUNC(bool, add, unsigned, const std::string&)
OOLUA_MEM_FUNC(void, play, unsigned)
OOLUA_PROXY_END


OOLUA_PROXY(GamepadComponent)//class has no bases
OOLUA_TAGS(OOLUA::No_public_constructors, OOLUA::Abstract, OOLUA::Register_class_enums)
OOLUA_ENUMS
(
	OOLUA_ENUM(Button_A)
	OOLUA_ENUM(Button_B)
	OOLUA_ENUM(Button_X)
	OOLUA_ENUM(Button_Y)
	OOLUA_ENUM(Button_LEFT)
	OOLUA_ENUM(Button_RIGHT)
	OOLUA_ENUM(Button_UP)
	OOLUA_ENUM(Button_DOWN)
)
OOLUA_MFUNC(is_button_pressed)
OOLUA_PROXY_END

OOLUA_PROXY(vector2df)
OOLUA_MGET(x, get_x)
OOLUA_MGET(y, get_y)
OOLUA_PROXY_END

OOLUA_PROXY(SpritebodyComponent)
OOLUA_TAGS(OOLUA::No_public_constructors, OOLUA::Abstract)
//OOLUA_MFUNC(apply_impulse)
//OOLUA_MFUNC(set_lineal_velocity)
OOLUA_MEM_FUNC(void, apply_impulse, float, float)
OOLUA_MEM_FUNC(void, set_lineal_velocity, float, float)
OOLUA_MEM_FUNC(void, apply_force, float, float)
OOLUA_MFUNC_CONST(get_lineal_velocity)
OOLUA_PROXY_END


OOLUA_PROXY(Pakal::LogMgr)
OOLUA_TAGS(OOLUA::No_public_constructors, OOLUA::Abstract, OOLUA::No_public_destructor)
OOLUA_MEM_FUNC(void, log, const std::string&)
OOLUA_PROXY_END
#endif