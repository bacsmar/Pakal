#include "ScriptComponent_Lua.h"
#include "ResourceManager.h"
#include "LuaProxies.h"
#include "PakalMath.h"

#include "luaintf/LuaIntf.h"
#include "lua.h"
#include "oolua.h"

//#include "components/ScriptComponent_Lua.h"
//#include "components/SpriteComponent.h"
//#include "components/SpritePhysicsComponent.h"
//#include "components/SFXComponent.h"
//#include "components/GamepadComponent.h"
//#include "IUIManager.h"
#include "SimpleTimer.h"

using namespace Pakal::tmath;


#if PAKAL_USE_SCRIPTS == 1

class ScriptResultLua : public Pakal::ScriptResult
{
	friend class ScriptComponentLua;
	LuaIntf::LuaState* m_script = nullptr;	
	bool m_is_ok = false;
public:
	explicit ScriptResultLua(LuaIntf::LuaState* script, bool ok) : m_script(script), m_is_ok(ok) {}
	ScriptResultLua(ScriptResultLua &&) = default;
	ScriptResultLua(ScriptResultLua &) = delete;

	virtual bool is_ok() const override
	{
		return m_is_ok;
	}

	operator bool() const override
	{		
		return m_script->popValue<bool>();
	};
	operator int() const override
	{
		return m_script->popValue<int>();
	};
	operator float() const override
	{
		return m_script->popValue<float>();
	};
	operator char() const override
	{
		return m_script->popValue<char>();
	};
	operator std::string() const override
	{
		return m_script->popValue<std::string>();
	};
};

template <class T, class TReturn, class ...Args>
auto fn_type_cast(TReturn(T::* c)(Args...args)) -> decltype (c)
{
	return c;
}

Pakal::ScriptComponentLua::ScriptComponentLua() : ScriptComponent(static_cast<Script*>(this))
{		
	//m_state = luaL_newstate();
	//luaL_openlibs(m_state);	

	oolua = new OOLUA::Script();	
	m_state = oolua->state();

	m_script = new LuaIntf::LuaState(m_state);

	// Note: these are the default components available to every script
	// maybe you're asking yourself where are being registered the other basic pakal::Components? 
	// if so: take a look at LuaProxies.cpp

	LuaIntf::LuaBinding( m_state ).beginClass<Pakal::tmath::vector2df>("Vector2df")
		.addVariable("x", &Pakal::tmath::vector2df::x)
		.addVariable("y", &Pakal::tmath::vector2df::y)
		.endClass();	

	LuaIntf::LuaBinding(m_state).beginClass<Pakal::tmath::vector3df>("Vector3df")
		.addVariable("x", &Pakal::tmath::vector3df::x)
		.addVariable("z", &Pakal::tmath::vector3df::z)
		.addVariable("y", &Pakal::tmath::vector3df::y)
		.endClass();	
	
	LuaIntf::LuaBinding(m_state).beginClass<Pakal::Path>("Path")
		.addConstructor(LUA_ARGS(LuaIntf::_opt<const std::string&>))
		.endClass();

	LuaIntf::LuaBinding( m_state ).beginClass<Pakal::SimpleTimer>("SimpleTimer")
		.addFunction("set_interval", &SimpleTimer::set_interval)
		.addFunction("expired", &SimpleTimer::expired)
		.endClass();

	LuaIntf::LuaBinding(m_state).beginClass<Pakal::Clock>("Clock")
		.addFunction("get_elapsed_time", &Pakal::Clock::getElapsedTime)
		.addFunction("restart", &Pakal::Clock::restart)
		.endClass();

	LuaIntf::LuaBinding(m_state).beginClass<Pakal::Time>("Time")
		.addFunction("as_milliseconds", &Pakal::Time::asMilliseconds)
		.addFunction("as_seconds", &Pakal::Time::asSeconds)
		.endClass();

	// system.disable_log()
	LuaIntf::LuaBinding(m_state).beginModule("system").addFunction("disable_log", [this]()
	{
		LuaIntf::LuaBinding(m_state).beginModule("system").addFunction("log", [](const char* s) {});
	});
	// system.enable_log()
	LuaIntf::LuaBinding(m_state).beginModule("system").addFunction("enable_log", [this]()
	{
		LuaIntf::LuaBinding(m_state).beginModule("system").addFunction("log", [](const char* s)
		{
			LOG_INFO("[script] %s", s);
		});
	});

	m_script->doString("system.enable_log();");

}

Pakal::ScriptComponentLua::~ScriptComponentLua()
{
	SAFE_DEL(m_script);
	SAFE_DEL(oolua);	
}

void Pakal::ScriptComponentLua::set_script(const std::string& script_chunk)
{
	mutex_guard lock(m_register_mutex);	
	oolua->run_chunk(script_chunk);
}

void Pakal::ScriptComponentLua::load_script(const std::string& script_file)
{
	mutex_guard lock(m_register_mutex);
	m_script_file = script_file;
	auto resource = ResourceMgr.open_read_resource(m_script_file, false);
	if (!resource.get())
		return;

	std::string str;
	file_utils::read_to_string(*resource, str);		
	oolua->run_chunk(str);
}

Pakal::ScriptResultPtr Pakal::ScriptComponentLua::call_function(const std::string& function)
{
	auto result = oolua->call(function);
	if (result == true)
	{	
		m_script->gc();
	}
	else
	{
		auto error = OOLUA::get_last_error(*m_script);
		LOG_ERROR("[ScriptComponent] lua_error: %s in %s", error.c_str(), m_script_file.c_str());
	}		
	
	return ScriptResultPtr( new ScriptResultLua(m_script, result));
}

Pakal::ScriptResultPtr Pakal::ScriptComponentLua::call_script(const std::string& script_text)
{
	auto result = oolua->run_chunk(script_text);
	if (result == true)
	{
		m_script->gc();
	}
	else
	{
		auto error = OOLUA::get_last_error(*m_script);
		LOG_ERROR("[ScriptComponent] lua_error: %s in %s", error.c_str(), m_script_file.c_str() );
	}	
	return ScriptResultPtr(new ScriptResultLua(m_script, result));
}

#endif
