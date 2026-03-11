#include "Engine.h"
#include "GameModuleLoader.h"
#include "LogMgr.h"
#include "ResourceManager.h"

#include <iostream>
#include <filesystem>

int main(int argc, char** argv)
{
	using namespace Pakal;

#if defined(PAKAL_WIN32_PLATFORM)
	const char* default_module_path = "ContraGameModule.dll";
#else
	const char* default_module_path = "./libContraGameModule.so";
#endif

	const char* module_path = argc > 1 ? argv[1] : default_module_path;

	namespace fs = std::filesystem;
	fs::path root_path = fs::current_path();
	fs::path module_candidate(module_path);
	if (module_candidate.has_parent_path())
	{
		root_path = fs::absolute(module_candidate.parent_path());
	}
	else if (argv[0])
	{
		fs::path exe_candidate(argv[0]);
		if (exe_candidate.has_parent_path())
		{
			root_path = fs::absolute(exe_candidate.parent_path());
		}
	}

	ResourceMgr.set_root_path(root_path.string());

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
