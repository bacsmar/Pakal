// The full engine's systems: what Engine::Settings picks by default when the engine is built with its backends (the
// desktop's Pakal library and the Android build). The core alone (PakalCore) uses source/core_only/EngineCoreDefaults.cpp
// instead; each file builds only in its own configuration, so a build that globs both still defines the constructor once.
#include "Config.h"

#ifndef PAKAL_CORE_ONLY

#include "Engine.h"
#include "SoundManager.h"
#include "DummyOSManagers.h"
#include "bgfx/BgfxGraphicsSystem.h"
#include "bgfx/BgfxRmlUi.h"
#include "box2D/Box2DPhysicsSystem.h"

#if PAKAL_USE_SFML_AUDIO == 1
	#include "sfml/SoundManagerSFML.h"
#endif
#if PAKAL_USE_SFML_INPUT == 1
	#include "sfml/InputManager_SFML.h"
#endif
#if PAKAL_USE_SFML_WINDOW == 1
	#include "sfml/WindowCreatorSFML.h"
#endif

using namespace Pakal;

Engine::Settings::Settings()
{
	// The picture: bgfx, and the game's interface, RmlUi drawn with bgfx
	graphic_system_settings.ui_manager_allocator = [](GraphicsSystem* gs) { return new BgfxRmlUi(static_cast<BgfxGraphicsSystem*>(gs)); };
	graphic_system_allocator = [](Engine*, const GraphicsSystem::Settings& settings) { return new BgfxGraphicsSystem(settings); };

	physics_system_allocator = [](Engine*, const PhysicsSystem::Settings& settings) { return new Box2DPhysicsSystem(settings); };

	// The sound, the window and the input: SFML's on the desktop. A platform without SFML (Android) starts with ones that
	// do nothing and sets its own after this.
#if PAKAL_USE_SFML_AUDIO == 1
	sound_manager_allocator = [](Engine*) { return new SoundManagerSFML(); };
#else
	sound_manager_allocator = [](Engine*) { return new NullSoundManager(); };
#endif

#if PAKAL_USE_SFML_INPUT == 1
	os_manager_settings.input_manager_allocator = [](OSManager* manager) { return new InputManager_SFML(manager); };
#else
	os_manager_settings.input_manager_allocator = [](OSManager*) { return new DummyInputManager(); };
#endif

#if PAKAL_USE_SFML_WINDOW == 1
	#ifdef PAKAL_ANDROID_PLATFORM
		os_manager_settings.window_manager_allocator = [](OSManager* manager) { return new WindowCreatorSFML(manager, true); };
	#else
		os_manager_settings.window_manager_allocator = [](OSManager* manager) { return new WindowCreatorSFML(manager, false); };
	#endif
#else
	os_manager_settings.window_manager_allocator = [](OSManager*) { return new DummyWindowManager(); };
#endif
}

#endif // PAKAL_CORE_ONLY
