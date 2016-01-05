#include "ScriptComponent.h"
#include "ResourceManager.h"
#include "LuaProxies.h"

#include "OOlua.h"
//#include "selene.h"

using namespace Pakal::tmath;

Pakal::ScriptComponent::ScriptComponent()
{
	m_script = new OOLUA::Script();
#if PAKAL_USE_SCRIPTS == 1
	m_script->register_class<OOLUA::vector2df>();
	m_script->register_class<OOLUA::Path>();
	m_script->register_class<Pakal::LogMgr>();

	auto result = OOLUA::push(m_script->state(), &LogMgr::instance(), OOLUA::Cpp);

	if (result)
		lua_setglobal(m_script->state(), "Log");

	//m_state = new sel::State();
#endif
}

Pakal::ScriptComponent::~ScriptComponent()
{
	SAFE_DEL(m_script);
	//SAFE_DEL(m_state);
}

void Pakal::ScriptComponent::set_script(const std::string& script_chunk)
{
	mutex_guard lock(m_register_mutex);
	m_script->run_chunk(script_chunk);
}

void Pakal::ScriptComponent::load_script(const std::string& script_file)
{
	mutex_guard lock(m_register_mutex);
	m_script_file = script_file;
	auto resource = ResourceMgr.open_read_resource(m_script_file, false);
	if (!resource.get())
		return;

	std::string str;
	file_utils::read_to_string(*resource, str);	
	m_script->run_chunk(str);		
}

Pakal::Selector Pakal::ScriptComponent::call_function(const std::string& function)
{
	auto result = m_script->call(function);
	if (result == true)
	{
		m_script->gc();
	}
	else
	{
		auto error = OOLUA::get_last_error(*m_script);
		LOG_ERROR("[ScriptComponent] lua_error: %s", error.c_str());
	}	
	return std::move (Selector(m_script, result));
}

Pakal::Selector Pakal::ScriptComponent::call_script(const std::string& script_text)
{
	//auto text = "function result() " + script_text + " end";
	auto result = m_script->run_chunk(script_text);
	if (result == true)
	{
		m_script->gc();
	}
	else
	{
		auto error = OOLUA::get_last_error(*m_script);
		LOG_ERROR("[ScriptComponent] lua_error: %s", error.c_str());
	}	
	return std::move(Selector(m_script, result));
}

const std::string Pakal::ScriptComponent::default_name_space = "Var";

void Pakal::ScriptComponent::push_integer(int value, const std::string& name, const std::string& name_space)
{	
	auto L = (m_script->state());

	lua_getglobal(L, name_space.c_str() );
	int luaNamespace = lua_gettop(L);

	if (lua_isnil(L, luaNamespace))
	{
		std::string s = name_space + " = " + name_space + " or {}";
		luaL_dostring(L, s.c_str() ); //register the "namespace" in fact is a lua table
	}

	lua_getglobal(L, name_space.c_str());
	luaNamespace = lua_gettop(L);

	ASSERT(lua_isnil(L, luaNamespace) == false);

	lua_pushinteger(L, value);	
	lua_setfield(L, luaNamespace, name.c_str());
}

Pakal::Selector::operator bool() const
{
	bool value = false;
	m_script->pull(value);
	return value;
}

Pakal::Selector::operator int() const
{
	int value = 0;
	m_script->pull(value);
	return value;
}


Pakal::Selector::operator float() const
{
	float value = 0;
	m_script->pull(value);
	return value;
}
Pakal::Selector::operator char() const
{
	char value = 0;
	m_script->pull(value);
	return value;
}
Pakal::Selector::operator std::string() const
{
	std::string value;
	m_script->pull(value);
	return std::move( value);
}