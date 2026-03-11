#include "ContraGame.h"
#include "Engine.h"
#include "GameModuleAPI.h"

namespace
{
	const char* get_module_name()
	{
		return "ContraGameModule";
	}

	Pakal::IPakalApplication* create_application()
	{
		return new Pakal::ContraGame();
	}

	void destroy_application(Pakal::IPakalApplication* application)
	{
		delete application;
	}

	void register_game_factories(void* engine)
	{
		(void)engine;
	}

	const Pakal::GameModuleExports g_contra_game_exports =
	{
		Pakal::PAKAL_GAME_MODULE_API_VERSION,
		&get_module_name,
		&create_application,
		&destroy_application,
		&register_game_factories
	};
}

PAKAL_GAME_MODULE_EXPORT const Pakal::GameModuleExports* pakal_get_game_module_exports()
{
	return &g_contra_game_exports;
}
