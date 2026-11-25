///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ExampleModule.cpp
// Purpose: The smallest game module of a game kept in its own repository (docs/GAME_REPOSITORY.md):
//          PakalPlayer ExampleModule logs its data file and runs until the window is closed.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "GameModuleAPI.h"
#include "IPakalApplication.h"
#include "LogMgr.h"
#include "ResourceManager.h"

namespace
{
	class ExampleApplication : public Pakal::IPakalApplication
	{
	public:
		const char* get_name() override { return "Example game"; }

		void start(Pakal::Engine& engine) override
		{
			(void)engine;
			bool found = false;
			const std::string text = ResourceMgr.read_text("Assets/example/hello.txt", &found);
			LOG_INFO("[ExampleModule] started; Assets/example/hello.txt %s", found ? text.c_str() : "is missing");
		}

		void end(Pakal::Engine& engine) override
		{
			(void)engine;
			LOG_INFO("[ExampleModule] ended");
		}
	};

	const char* get_module_name() { return "ExampleModule"; }
	Pakal::IPakalApplication* create_application() { return new ExampleApplication(); }
	void destroy_application(Pakal::IPakalApplication* application) { delete application; }
	void register_game_factories(void* engine) { (void)engine; }

	const Pakal::GameModuleExports g_example_exports =
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
	return &g_example_exports;
}
