#include "Engine.h"
#include "GameModuleLoader.h"
#include "LogMgr.h"
#include "ResourceManager.h"
#include "GraphicsSystem.h"
#include "OSManager.h"
#include "bgfx/BgfxGraphicsSystem.h"
#include "mcp/EngineTools.h"

#include <cstdio>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cwctype>
#endif
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <memory>
#include <fstream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv)
{
	using namespace Pakal;

#if defined(PAKAL_WIN32_PLATFORM)
	const char* default_module_path = "ContraGameModule.dll";
#else
	const char* default_module_path = "./libContraGameModule.so";
#endif

	// Without a module on the command line (the player started by a double click on a copy of it renamed for a game), the module is the
	// one `pakal_module.txt` names, next to the player
	std::string named_module;
	if (argc <= 1)
	{
		std::filesystem::path beside = argv[0] && std::filesystem::path(argv[0]).has_parent_path() ? std::filesystem::absolute(std::filesystem::path(argv[0]).parent_path()) : std::filesystem::current_path();
		std::ifstream file(beside / "pakal_module.txt");
		if (file && std::getline(file, named_module))
		{
			while (!named_module.empty() && (named_module.back() == '\r' || named_module.back() == ' ')) named_module.pop_back();
			if (!named_module.empty()) named_module = (beside / named_module).string();
		}
		// and its environment, next to it too (pakal_env.txt: KEY=VALUE lines; what the environment already says wins)
		std::ifstream env(beside / "pakal_env.txt");
		for (std::string line; env && std::getline(env, line);)
		{
			while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
			const size_t equals = line.find('=');
			if (equals == std::string::npos || line[0] == '#' || std::getenv(line.substr(0, equals).c_str())) continue;
#if defined(_WIN32)
			_putenv_s(line.substr(0, equals).c_str(), line.substr(equals + 1).c_str());
#else
			setenv(line.substr(0, equals).c_str(), line.substr(equals + 1).c_str(), 0);
#endif
		}
	}
	const char* module_path = argc > 1 ? argv[1] : (!named_module.empty() ? named_module.c_str() : default_module_path);
	// The options after the module, for it to read as variables: --edit [level.json] opens a TopDown module's level editor
	// (without a level: on its chooser, PAKAL_EDIT=*)
	for (int i = 2; i < argc; ++i)
	{
		if (std::string(argv[i]) == "--edit")
		{
			const char* level = i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0 ? argv[i + 1] : "*";
#if defined(_WIN32)
			_putenv_s("PAKAL_EDIT", level);
#else
			setenv("PAKAL_EDIT", level, 1);
#endif
		}
	}

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
#if defined(_WIN32)
	// the user's language (two letters: "es-MX" is "es"), for a game's texts
	{
		wchar_t locale[LOCALE_NAME_MAX_LENGTH] = {};
		if (GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH) > 2)
			OSMgr.set_language(std::string{ static_cast<char>(towlower(locale[0])), static_cast<char>(towlower(locale[1])) });
	}
#endif

	std::cout << "============================" << std::endl;
	std::cout << " Pakal Player" << std::endl;
	std::cout << "============================" << std::endl;

	try
	{
		// Each system runs at its own rate: physics and the game's logic at 120 (short steps, positions at most ~8 ms old
		// when the picture takes them), the picture at PAKAL_FPS_CAP (60 by default; 0 removes the cap), PAKAL_VSYNC=0
		// disables vsync. PAKAL_LOGIC_FPS / PAKAL_PHYSICS_FPS change the other two. The FPS of every system is logged
		// once per second (PAKAL_FPS_LOG=1).
		const char* capText = std::getenv("PAKAL_FPS_CAP");
		const int cap = capText ? std::atoi(capText) : 60;
		const int maxFps = cap > 0 ? cap : 100000;
		const char* vsyncText = std::getenv("PAKAL_VSYNC");
		const bool vsync = vsyncText ? std::atoi(vsyncText) != 0 : true;

		Engine::Settings settings;
		auto rate = [](const char* name, int fallback) { const char* text = std::getenv(name); return text && std::atoi(text) > 0 ? std::atoi(text) : fallback; };
		settings.max_fps = rate("PAKAL_LOGIC_FPS", 120);
		settings.graphic_system_settings.max_fps = maxFps;
		settings.graphic_system_settings.resolution = {1280, 720};
		// PAKAL_RESOLUTION=WxH changes the window size (to tell pixel cost from geometry cost)
		if (const char* resolution = std::getenv("PAKAL_RESOLUTION"))
		{
			int width = 0, height = 0;
			if (std::sscanf(resolution, "%dx%d", &width, &height) == 2 && width > 0 && height > 0)
			{
				settings.graphic_system_settings.resolution = {width, height};
			}
		}
		settings.graphic_system_settings.vsync = vsync;
		if (const char* no3d = std::getenv("PAKAL_NO_3D")) settings.graphic_system_settings.no_3d = std::string(no3d) == "1";
		settings.graphic_system_settings.full_screen = false;
		settings.physic_system_settings.gravity = tmath::vector3df(0.0f, -20.0f, 0.0f);
		settings.physic_system_settings.max_fps = rate("PAKAL_PHYSICS_FPS", 120);

		Engine engine(settings);
		GameModuleLoader module;
		if (!module.load(module_path))
		{
			LOG_ERROR("[PakalPlayer] Failed to load game module from '%s'", module_path);
			return 1;
		}

		LOG_INFO("[PakalPlayer] Loaded module: %s", module.module_name());
		module.register_game_factories(&engine);

		// Optional MCP server with the engine's tools (stats, settings, camera, screenshots; source/mcp/EngineTools):
		// PAKAL_MCP_PORT=<port> (127.0.0.1 only); PAKAL_MCP_TOKEN makes it require "Authorization: Bearer <token>". Off by
		// default.
		std::unique_ptr<McpServer> mcp = start_engine_tools(engine);

		engine.run(module.application());
	}
	catch (const std::exception& e)
	{
		std::cerr << "Fatal error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
