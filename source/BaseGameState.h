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
		inline const char* get_name()
		{
			return m_name.c_str();
		}

	private:
		GameStateManager* m_game_state_manager;
		bool m_deallocate_on_pop;
		std::string m_name;

	protected:

		explicit BaseGameState(const std::string& name): m_game_state_manager(nullptr), m_deallocate_on_pop(false)
		{
			m_name = name;
		}

		virtual ~BaseGameState() { }

		inline const GameStateManager* get_game_state_manager()
		{
			return m_game_state_manager;
		};

		virtual void initialize(Engine* engine) = 0;
		virtual void terminate() {};
		virtual void on_resume() {};
		virtual void on_pause() {};
	};
}