#pragma once

#include "ScriptComponent.h"
#include <string>
#include "TaskFwd.h"

#include "luaintf/LuaIntf.h"
#include "components/CppBindClass_lua.h"

namespace OOLUA
{
	class Script;
}

namespace Pakal 
{
	class ScriptComponentLua : public ScriptComponent
	{
		DECLARE_RTTI_WITH_BASE(ScriptComponentLua, ScriptComponent);

		LuaIntf::LuaState * m_script = nullptr;
		lua_State* m_state;

		OOLUA::Script* oolua;

		std::string m_script_file;
		std::mutex	m_register_mutex;		
	public:
		ScriptComponentLua();
		~ScriptComponentLua();

		void set_script(const std::string& script_chunk) override;
		void load_script(const std::string& script_file) override;
		
		ScriptResultPtr call_function(const std::string& function) override;
		ScriptResultPtr call_script(const std::string& script_text) override;	

		template <typename T>
		inline void register_obj(const std::string& name, T *t)
		{			
			auto& state = *m_script;
			state.push(t);
			state.setGlobal(name.c_str());
		}

		template <typename T>
		inline auto register_class(const std::string& name)  -> CppBindClass< T, LuaIntf::CppBindClass<T, LuaIntf::CppBindModule<LuaIntf::LuaBinding>> >
		{
			auto clazz = LuaIntf::LuaBinding( *m_script).beginModule("Classes").beginClass<T>(name.c_str());
			return CppBindClass<T,decltype(clazz)>::bind(clazz);
		}

		template<typename T>
		inline void register_lambda(const std::string& name,  T&& fn)
		{
			LuaIntf::LuaBinding(*m_script).addFunction(name.c_str(), fn);
		}
	};	

#if PAKAL_USE_SCRIPTS == 1
	//using Script = ScriptComponentLua;
	class Script : public ScriptComponentLua{};
#endif

}