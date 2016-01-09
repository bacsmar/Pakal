#pragma once

#include "Component.h"
#include <string>
#include "TaskFwd.h"

#include "LuaState/include/LuaState.h"

struct lua_State;
namespace OOLUA
{
	class Script;	
}

namespace Pakal 
{
	class Selector
	{		
		friend class ScriptComponent;
		OOLUA::Script* m_script = nullptr;		
		bool m_is_ok;
		explicit Selector(OOLUA::Script* script, bool ok) : m_script(script), m_is_ok(ok) {}
	public:
		Selector(Selector &&) = default; 
		Selector(Selector &) = delete;
		operator bool() const;
		operator int() const;
		operator float() const;
		operator char() const;
		operator std::string() const;
		inline bool is_ok() const { return m_is_ok; }		
	};
	//helper function used to register components in script
	//template <class T>

	template <class T>
	void oolua_script_interface_for(T*, OOLUA::Script&, const std::string& );
	
	// Scriptable
	class ScriptComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(ScriptComponent, Component);

		OOLUA::Script* m_script = nullptr;
		lua::State* m_aux_script = nullptr;
		std::string m_script_file;
		std::mutex	m_register_mutex;

		static const std::string default_name_space;		
	public:
		ScriptComponent();
		~ScriptComponent();

		void set_script(const std::string& script_chunk);
		void load_script(const std::string& script_file);
		
		Selector call_function(const std::string& function);		
		Selector call_script(const std::string& script_text);

		template <class T>
		void register_on_script(T* obj, const std::string& name)
		{
			mutex_guard lock(m_register_mutex);
#if PAKAL_USE_SCRIPTS == 1			
			oolua_script_interface_for(obj, *m_script, name);
#endif
		}		

		template <class T>
		void push_value(T value, const std::string& name, const std::string& name_space = default_name_space)
		{
			if( std::is_integral<T>::value || std::is_enum<T>::value)
			{
				push_integer(static_cast<int>(value), name, name_space);
			}
		}
		void push_integer(int value, const std::string& name, const std::string& name_space = default_name_space);

		template<typename T>
		void set(const char* key, T value) const
		{
			m_aux_script->set(key, std::forward<T>(value));
		}
	};	
}