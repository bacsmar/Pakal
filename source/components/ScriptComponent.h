#pragma once

#include "Component.h"
#include <string>
#include "TaskFwd.h"

struct lua_State;
namespace OOLUA
{
	class Script;	
}
//namespace sel
//{
//	class State;
//}

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
		operator bool() const;
		operator int() const;
		operator float() const;
		operator char() const;
		operator std::string() const;
		inline bool is_ok() const { return m_is_ok; }		
	};
	//helper function used to register components in script
	//template <class T>
	//void selene_script_interface_for(T*, sel::State&, const std::string&);

	template <class T>
	void oolua_script_interface_for(T*, OOLUA::Script&, const std::string& );
	
	// Scriptable
	class ScriptComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(ScriptComponent, Component);

		OOLUA::Script* m_script = nullptr;
		std::string m_script_file;
		std::mutex	m_register_mutex;

		//sel::State* m_state;
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
			//selene_script_interface_for(obj, *m_state);
#if PAKAL_USE_SCRIPTS == 1
			oolua_script_interface_for(obj, *m_script, name);
#endif
		}
	};	
}