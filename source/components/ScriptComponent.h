#pragma once

#include "Component.h"
#include <string>
#include "TaskFwd.h"

struct lua_State;
namespace OOLUA
{
	class Script;	
}

namespace Pakal 
{
	class Script;

	class ScriptResult
	{		
	public:							
		virtual ~ScriptResult(){}
		ScriptResult(ScriptResult &&) = default;
		ScriptResult() = default;
		ScriptResult(ScriptResult &) = delete;

		virtual operator bool() const = 0;
		virtual operator int() const = 0;
		virtual operator float() const = 0;
		virtual operator char() const = 0;
		virtual operator std::string() const = 0;

		virtual bool is_ok() const = 0;
	};	

	using ScriptResultPtr = std::unique_ptr<ScriptResult>;

	template <class T>
	void register_script_interface_for(T*, Script&, const std::string&);
	
	// Scriptable
	class ScriptComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(ScriptComponent, Component);
		std::mutex	m_register_mutex;
	public:		

		virtual void set_script(const std::string& script_chunk) = 0;
		virtual void load_script(const std::string& script_file) = 0;
		
		virtual ScriptResultPtr call_function(const std::string& function) = 0;
		virtual ScriptResultPtr call_script(const std::string& script_text) = 0;

		template <class T>
		void register_in_script(T* obj, const std::string& name)
		{
			mutex_guard lock(m_register_mutex);
			register_script_interface_for(obj, *m_script_component_imp, name);
		}

		inline Script* get_script() const { return m_script_component_imp; }

	protected:		
		Script* m_script_component_imp;
		explicit ScriptComponent(Script* script) : m_script_component_imp(script){}
	};	

#if PAKAL_USE_SCRIPTS == 0	
	class DummyScriptComponent : public ScriptComponent
	{
		DECLARE_RTTI_WITH_BASE(DummyScriptComponent, ScriptComponent);
	public:

		class DummyResult : public ScriptResult{
		public:
			operator bool() const override { return false; };
			operator int() const override { return 0; };
			operator float() const override { return 0.f; };
			operator char() const override { return 0; };
			operator std::basic_string<char>() const override { return ""; };
			bool is_ok() const override { return false; };
		};

		void set_script(const std::string& script_chunk) override {} ;

		void load_script(const std::string& script_file) override{};

		ScriptResultPtr call_function(const std::string& function) override { return ScriptResultPtr(new DummyResult()); };

		ScriptResultPtr call_script(const std::string& script_text) override{ return ScriptResultPtr(new DummyResult()); };
		template<typename T>
		inline void register_lambda(const std::string& name, T&& fn){		}

		explicit DummyScriptComponent();
	};	

	class Script : public DummyScriptComponent {};

	inline DummyScriptComponent::DummyScriptComponent() : ScriptComponent(static_cast<Script*>(this))
	{}
	template <class T>
	void register_script_interface_for(T*, Script&, const std::string&){}
#endif
}