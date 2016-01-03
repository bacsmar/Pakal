#pragma once
#include "Config.h"
#if PAKAL_USE_SCRIPTS == 1
#include "BaseGameState.h"

namespace Pakal
{
	class ScriptComponent;

	class _PAKALExport ScriptableGameState : public BaseGameState
	{
		friend GameStateManager;
		ScriptComponent *m_script = nullptr;
		std::string m_script_file;
	protected:

		explicit ScriptableGameState(const std::string& name, const std::string& script_file);

		virtual ~ScriptableGameState();

		virtual void on_initialize(Engine* engine) override;
		virtual void on_terminate(Engine* engine) override;
		virtual void on_update() override;
		virtual void on_activate(Engine* engine) override;
		virtual void on_deactivate(Engine* engine) override;;
	};
}

#endif