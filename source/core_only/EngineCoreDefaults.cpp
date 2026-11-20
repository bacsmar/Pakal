// The core alone (PakalCore, PAKAL_CORE_ONLY): what Engine::Settings picks by default when the engine is built without
// its backends. Systems that do nothing take their place, so the core starts, runs and ends on its own (CoreTests). The
// full engine uses source/runtime/EngineBackends.cpp instead.
#include "Config.h"

#ifdef PAKAL_CORE_ONLY

#include "Engine.h"
#include "SoundManager.h"
#include "DummyOSManagers.h"
#include "DummyUIManager.h"
#include "DummyGraphicsSystem.h"
#include "DummyPhysicsSystem.h"

using namespace Pakal;

Engine::Settings::Settings()
{
	graphic_system_settings.ui_manager_allocator = [](GraphicsSystem*) { return new DummyUIManager(); };
	graphic_system_allocator = [](Engine*, const GraphicsSystem::Settings& settings) { return new DummyGraphicsSystem(settings); };
	physics_system_allocator = [](Engine*, const PhysicsSystem::Settings& settings) { return new DummyPhysicsSystem(settings); };
	sound_manager_allocator = [](Engine*) { return new NullSoundManager(); };
	os_manager_settings.input_manager_allocator = [](OSManager*) { return new DummyInputManager(); };
	os_manager_settings.window_manager_allocator = [](OSManager*) { return new DummyWindowManager(); };
}

#endif // PAKAL_CORE_ONLY
