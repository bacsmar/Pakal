///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: main.cpp
// Creation Date: January 2026
//
// Purpose: Entry point for Contra Game Example
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ContraGame.h"
#include "Engine.h"
#include "Config.h"
#include <cstdlib>

int main(int argc, char** argv)
{
	using namespace Pakal;

	// Configure engine settings
	Engine::Settings settings;

	// Graphics settings
	settings.graphic_system_settings.resolution = {1280, 720};
	settings.graphic_system_settings.vsync = true;
	settings.graphic_system_settings.full_screen = false;
	settings.graphic_system_settings.max_fps = 60;

	// Physics settings for platformer
	// Note: Gravity is typically negative Y in 2D platformers
	// settings.physic_system_settings.gravity = {0.0f, -20.0f};
	// settings.physic_system_settings.debug_draw = false;
	// settings.physic_system_settings.max_fps = 60;

#if PAKAL_USE_BGFX
	// bgfx backend configuration
	// TODO: Set up bgfx graphics system allocator
	// settings.graphic_system_allocator = [](Engine* engine, const GraphicsSystem::Settings& s) {
	//     return new BgfxGraphicsSystem(s);
	// };
#elif PAKAL_USE_IRRLICHT
	// Use Irrlicht backend (default)
	// No additional configuration needed
#else
	// Fallback to dummy graphics
	#warning "No graphics backend enabled. Using dummy graphics system."
#endif

	// Engine configuration
	settings.uses_thread = false;  // Single-threaded for simplicity
	settings.max_fps = 60;

	try
	{
		// Create engine
		Engine engine(settings);

		// Create and run game
		ContraGame game;
		engine.run(&game);

		return EXIT_SUCCESS;
	}
	catch (const std::exception& e)
	{
		// Log error
		// LOG_ERROR("Fatal error: %s", e.what());
		return EXIT_FAILURE;
	}
	catch (...)
	{
		// LOG_ERROR("Unknown fatal error occurred");
		return EXIT_FAILURE;
	}
}
