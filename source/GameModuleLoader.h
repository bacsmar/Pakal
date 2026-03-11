#pragma once

#include "Config.h"
#include "GameModuleAPI.h"

#include <string>

namespace Pakal
{
	class IPakalApplication;

	class _PAKALExport GameModuleLoader final
	{
	public:
		GameModuleLoader();
		~GameModuleLoader();

		bool load(const char* module_path);
		void unload();

		bool is_loaded() const;
		const char* module_name() const;
		IPakalApplication* application() const;
		void register_game_factories(void* engine) const;

	private:
		void* m_module_handle;
		const GameModuleExports* m_exports;
		IPakalApplication* m_application;
		std::string m_module_path;
	};
}
