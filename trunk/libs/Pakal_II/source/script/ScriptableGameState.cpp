#include "ScriptableGameState.h"

#if PAKAL_USE_SCRIPTS == 1
#include "Components/ScriptComponent.h"
#include "ResourceManager.h"
#include "Engine.h"
#include "ComponentManager.h"

Pakal::ScriptableGameState::ScriptableGameState(const std::string& name, const std::string& script_file): BaseGameState(name), m_script_file(script_file)
{	
}

Pakal::ScriptableGameState::~ScriptableGameState()
{	
}

void Pakal::ScriptableGameState::on_initialize(Engine* engine)
{
	auto resource = ResourceMgr.open_read_resource(m_script_file);

	ASSERT(resource->good());

	std::string script;
	file_utils::read_to_string(*resource, script);

	m_script = engine->component_manager()->create_component<ScriptComponent>();

	m_script->load_script(script);
	//m_script->register_in_script(engine->get_ui_manager(), "UI");

	m_script->call_function("on_initialize");
}

void Pakal::ScriptableGameState::on_terminate(Engine* engine)
{
	m_script->call_function("on_terminate");
	SAFE_DEL(m_script);
}

void Pakal::ScriptableGameState::on_update()
{
	m_script->call_function("on_update");
}

void Pakal::ScriptableGameState::on_activate(Engine* engine)
{
	m_script->call_function("on_activate");
}

void Pakal::ScriptableGameState::on_deactivate(Engine* engine)
{
	m_script->call_function("on_deactivate");
}

#endif //PAKAL_USE_SCRIPTS