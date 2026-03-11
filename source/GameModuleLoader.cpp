#include "GameModuleLoader.h"

#include "IPakalApplication.h"
#include "LogMgr.h"

#if defined(PAKAL_WIN32_PLATFORM)
	#include <Windows.h>
#else
	#include <dlfcn.h>
#endif

using namespace Pakal;

namespace
{
	void* open_module(const char* module_path)
	{
#if defined(PAKAL_WIN32_PLATFORM)
		return reinterpret_cast<void*>(LoadLibraryA(module_path));
#else
		// Use RTLD_LAZY to delay symbol resolution, and RTLD_GLOBAL to make symbols available to other modules
		// This allows dynamic modules to use symbols from statically-linked Pakal in PakalPlayer
		return dlopen(module_path, RTLD_LAZY | RTLD_GLOBAL);
#endif
	}

	void close_module(void* module_handle)
	{
#if defined(PAKAL_WIN32_PLATFORM)
		if (module_handle)
		{
			FreeLibrary(reinterpret_cast<HMODULE>(module_handle));
		}
#else
		if (module_handle)
		{
			dlclose(module_handle);
		}
#endif
	}

	void* load_symbol(void* module_handle, const char* symbol_name)
	{
#if defined(PAKAL_WIN32_PLATFORM)
		return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(module_handle), symbol_name));
#else
		return dlsym(module_handle, symbol_name);
#endif
	}

	const char* get_module_error()
	{
#if defined(PAKAL_WIN32_PLATFORM)
		return "Failed to load module or symbol on Windows";
#else
		const char* error = dlerror();
		return error ? error : "unknown dlopen/dlsym error";
#endif
	}
}

GameModuleLoader::GameModuleLoader() :
	m_module_handle(nullptr),
	m_exports(nullptr),
	m_application(nullptr)
{
}

GameModuleLoader::~GameModuleLoader()
{
	unload();
}

bool GameModuleLoader::load(const char* module_path)
{
	ASSERT(module_path);

	if (is_loaded())
	{
		LOG_WARNING("[GameModuleLoader] Module already loaded: %s", m_module_path.c_str());
		return false;
	}

	m_module_handle = open_module(module_path);
	if (!m_module_handle)
	{
		LOG_ERROR("[GameModuleLoader] Unable to open module '%s': %s", module_path, get_module_error());
		return false;
	}

	auto get_exports = reinterpret_cast<GetGameModuleExportsFn>(load_symbol(m_module_handle, "pakal_get_game_module_exports"));
	if (!get_exports)
	{
		LOG_ERROR("[GameModuleLoader] Missing symbol 'pakal_get_game_module_exports' in '%s': %s", module_path, get_module_error());
		close_module(m_module_handle);
		m_module_handle = nullptr;
		return false;
	}

	m_exports = get_exports();
	if (!m_exports)
	{
		LOG_ERROR("[GameModuleLoader] Module '%s' returned null exports", module_path);
		close_module(m_module_handle);
		m_module_handle = nullptr;
		return false;
	}

	if (m_exports->api_version != PAKAL_GAME_MODULE_API_VERSION)
	{
		LOG_ERROR("[GameModuleLoader] ABI version mismatch for '%s'. Module=%u Engine=%u",
			module_path,
			m_exports->api_version,
			PAKAL_GAME_MODULE_API_VERSION);
		close_module(m_module_handle);
		m_module_handle = nullptr;
		m_exports = nullptr;
		return false;
	}

	if (!m_exports->create_application || !m_exports->destroy_application)
	{
		LOG_ERROR("[GameModuleLoader] Module '%s' has incomplete exports", module_path);
		close_module(m_module_handle);
		m_module_handle = nullptr;
		m_exports = nullptr;
		return false;
	}

	m_application = m_exports->create_application();
	if (!m_application)
	{
		LOG_ERROR("[GameModuleLoader] Module '%s' failed to create application instance", module_path);
		close_module(m_module_handle);
		m_module_handle = nullptr;
		m_exports = nullptr;
		return false;
	}

	m_module_path = module_path;
	LOG_INFO("[GameModuleLoader] Loaded module '%s'", module_path);
	return true;
}

void GameModuleLoader::unload()
{
	if (m_exports && m_application && m_exports->destroy_application)
	{
		m_exports->destroy_application(m_application);
	}

	m_application = nullptr;
	m_exports = nullptr;

	if (m_module_handle)
	{
		close_module(m_module_handle);
		m_module_handle = nullptr;
	}

	m_module_path.clear();
}

bool GameModuleLoader::is_loaded() const
{
	return m_module_handle != nullptr && m_exports != nullptr && m_application != nullptr;
}

const char* GameModuleLoader::module_name() const
{
	if (!m_exports || !m_exports->get_name)
	{
		return "UnknownGameModule";
	}

	return m_exports->get_name();
}

IPakalApplication* GameModuleLoader::application() const
{
	return m_application;
}

void GameModuleLoader::register_game_factories(void* engine) const
{
	if (m_exports && m_exports->register_game_factories)
	{
		m_exports->register_game_factories(engine);
	}
}
