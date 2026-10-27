///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: main.cpp
// Purpose: Entry point for Contra game example
///////////////////////////////////////////////////////////////////////////////////////////////////////

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
	
	std::cout << "====================================" << std::endl;
	std::cout << " Pakal Engine - Contra Game Example" << std::endl;
	std::cout << "====================================" << std::endl;
	
	try
	{
		// Create engine settings
		Engine::Settings settings;
		
		// FPS limits
		settings.max_fps = 60;  // Engine main loop at 60 FPS
		settings.graphic_system_settings.max_fps = 60;  // Graphics at 60 FPS
		
		// Graphics: use bgfx
		settings.graphic_system_settings.resolution = {1280, 720};
		settings.graphic_system_settings.vsync = true;
		settings.graphic_system_settings.full_screen = false;
		
		// Physics: use Box2D for 2D
		settings.physic_system_settings.gravity = tmath::vector3df(0.0f, -20.0f, 0.0f); // Platformer gravity
		settings.physic_system_settings.max_fps = 60;
		
		LOG_INFO("[ContraGame] Creating engine with resolution 1280x720");
		
		// Create engine
		Engine engine(settings);
		
		GameModuleLoader module;
		if (!module.load(module_path))
		{
			LOG_ERROR("[ContraGame] Failed to load game module from '%s'", module_path);
			return 1;
		}

		LOG_INFO("[ContraGame] Loaded module: %s", module.module_name());
		module.register_game_factories(&engine);
		engine.run(module.application());
		
		LOG_INFO("[ContraGame] Game exited normally");
	}
	catch (const std::exception& e)
	{
		std::cerr << "Fatal error: " << e.what() << std::endl;
		return 1;
	}
	
	return 0;
}
