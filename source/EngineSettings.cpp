#include "Engine.h"
#include "persist/Archive.h"

// Engine::Settings::Settings() picks each system's implementation, so it lives with the implementations it names:
// source/runtime/EngineBackends.cpp in the full engine (bgfx, Box2D, SFML or the platform's own), and
// source/core_only/EngineCoreDefaults.cpp in the core alone (PakalCore: the dummy systems). This file, the core's, knows
// none of them.

using namespace Pakal;

void Engine::Settings::persist(Archive* archive)
{
	archive->value("uses_thread", uses_thread);
	archive->value("max_fps", max_fps);
	archive->value("physic_system_settings", physic_system_settings);
	archive->value("graphic_system_settings", graphic_system_settings);
}
