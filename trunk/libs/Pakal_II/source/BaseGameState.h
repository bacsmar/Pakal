#pragma once
#include "Config.h"
#include <string>

namespace Pakal
{
	class Engine;
	class GameStateManager;

	class _PAKALExport BaseGameState
	{
		friend GameStateManager;

	public:
		inline const char* get_name() { return m_name.c_str(); }

	private:
		GameStateManager* m_game_state_manager;
		bool m_deallocate_on_pop;
		std::string m_name;

	protected:

		explicit BaseGameState(const std::string& name): m_game_state_manager(nullptr), m_deallocate_on_pop(false), m_name(name) {}
		virtual ~BaseGameState() { }

		inline const GameStateManager* get_manager() { return m_game_state_manager; };

		virtual void on_initialize(Engine* engine) = 0;
		virtual void on_terminate(Engine* engine) = 0;
		virtual void on_update() = 0;
		virtual void on_resume(Engine* engine) = 0;
		virtual void on_pause(Engine* engine) = 0;;
	};
}