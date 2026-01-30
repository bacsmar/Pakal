///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: main.cpp
// Purpose: Entry point for Contra game example
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ContraGame.h"
#include "Engine.h"
#include "LogMgr.h"
#include <iostream>

int main(int argc, char** argv)
{
	using namespace Pakal;
	
	std::cout << "====================================" << std::endl;
	std::cout << " Pakal Engine - Contra Game Example" << std::endl;
	std::cout << "====================================" << std::endl;
	
	try
	{
		// Create engine settings
		Engine::Settings settings;
		
		// Graphics: use bgfx
		settings.graphic_system_settings.resolution = {1280, 720};
		settings.graphic_system_settings.vsync = true;
		settings.graphic_system_settings.full_screen = false;
		
		// Physics: use Box2D for 2D
		settings.physic_system_settings.gravity = {0.0f, -20.0f}; // Platformer gravity
		settings.physic_system_settings.max_fps = 60;
		
		LOG_INFO("[ContraGame] Creating engine with resolution 1280x720");
		
		// Create engine
		Engine engine(settings);
		
		// Create and run game
		ContraGame game;
		LOG_INFO("[ContraGame] Running game");
		engine.run(&game);
		
		LOG_INFO("[ContraGame] Game exited normally");
	}
	catch (const std::exception& e)
	{
		std::cerr << "Fatal error: " << e.what() << std::endl;
		return 1;
	}
	
	return 0;
}
