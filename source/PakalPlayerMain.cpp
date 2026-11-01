#include "Engine.h"
#include "GameModuleLoader.h"
#include "LogMgr.h"
#include "ResourceManager.h"
#include "GraphicsSystem.h"
#include "bgfx/BgfxGraphicsSystem.h"
#include "mcp/McpServer.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <memory>
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

	const char* module_path = argc > 1 ? argv[1] : default_module_path;
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

		// Optional MCP server for AI clients: PAKAL_MCP_PORT=<port> (127.0.0.1 only); PAKAL_MCP_TOKEN
		// makes it require "Authorization: Bearer <token>". Off by default.
		std::unique_ptr<McpServer> mcp;
		if (const char* portText = std::getenv("PAKAL_MCP_PORT"))
		{
			GraphicsSystem* graphics = engine.graphics_system();
			mcp = std::make_unique<McpServer>();
			mcp->add_tool("get_status", { "Frame rates of the engine and of the graphics system", nlohmann::json{ { "type", "object" }, { "properties", nlohmann::json::object() } },
				[&engine, graphics](const nlohmann::json&)
				{
					return nlohmann::json{ { "engine_fps", engine.get_fps() }, { "graphics_fps", graphics ? graphics->get_fps() : 0u } };
				} });
			mcp->add_tool("screenshot", { "Saves the next presented frame as a TGA file. Returns the path; the file appears on the following frame",
				nlohmann::json{ { "type", "object" }, { "properties", { { "path", { { "type", "string" }, { "description", "Destination .tga file" } } } } }, { "required", { "path" } } },
				[graphics](const nlohmann::json& arguments)
				{
					const std::string path = arguments.at("path").get<std::string>();
					if (path.size() < 5 || path.substr(path.size() - 4) != ".tga")
					{
						throw std::runtime_error("the path must end in .tga");
					}
					auto* bgfx = static_cast<BgfxGraphicsSystem*>(graphics);
					if (!bgfx)
					{
						throw std::runtime_error("no graphics system");
					}
					graphics->execute_block([bgfx, path]() { bgfx->request_screenshot(path.c_str()); });
					return nlohmann::json{ { "path", path } };
				} });
			std::string error;
			const unsigned short port = static_cast<unsigned short>(std::atoi(portText));
			const char* token = std::getenv("PAKAL_MCP_TOKEN");
			if (!mcp->start(port, token ? token : "", error))
			{
				LOG_ERROR("[PakalPlayer] MCP server not started: %s", error.c_str());
			}
		}

		engine.run(module.application());
	}
	catch (const std::exception& e)
	{
		std::cerr << "Fatal error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
