#include "Engine.h"
#include "GameModuleLoader.h"
#include "LogMgr.h"

#include <iostream>

int main(int argc, char** argv)
{
	using namespace Pakal;

#if defined(PAKAL_WIN32_PLATFORM)
	const char* default_module_path = "ContraGameModule.dll";
#else
	const char* default_module_path = "./libContraGameModule.so";
#endif

	const char* module_path = argc > 1 ? argv[1] : default_module_path;

	std::cout << "============================" << std::endl;
	std::cout << " Pakal Player" << std::endl;
	std::cout << "============================" << std::endl;

	try
	{
		Engine::Settings settings;
		settings.max_fps = 60;
		settings.graphic_system_settings.max_fps = 60;
		settings.graphic_system_settings.resolution = {1280, 720};
		settings.graphic_system_settings.vsync = true;
		settings.graphic_system_settings.full_screen = false;
		settings.physic_system_settings.gravity = tmath::vector3df(0.0f, -20.0f, 0.0f);
		settings.physic_system_settings.max_fps = 60;

		Engine engine(settings);
		GameModuleLoader module;
		if (!module.load(module_path))
		{
			LOG_ERROR("[PakalPlayer] Failed to load game module from '%s'", module_path);
			return 1;
		}

		LOG_INFO("[PakalPlayer] Loaded module: %s", module.module_name());
		module.register_game_factories(&engine);
		engine.run(module.application());
	}
	catch (const std::exception& e)
	{
		std::cerr << "Fatal error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
