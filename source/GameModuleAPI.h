#pragma once

#include <cstdint>
#include "Platform.h"

namespace Pakal
{
	class IPakalApplication;

	static constexpr uint32_t PAKAL_GAME_MODULE_API_VERSION = 1;

	struct GameModuleExports
	{
		uint32_t api_version;
		const char* (*get_name)();
		IPakalApplication* (*create_application)();
		void (*destroy_application)(IPakalApplication* application);
		void (*register_game_factories)(void* engine);
	};

	using GetGameModuleExportsFn = const GameModuleExports* (*)();
}

#if defined(PAKAL_WIN32_PLATFORM)
	#define PAKAL_GAME_MODULE_EXPORT extern "C" __declspec(dllexport)
#else
	#define PAKAL_GAME_MODULE_EXPORT extern "C" __attribute__((visibility("default")))
#endif
