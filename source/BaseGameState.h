#pragma once

#include <string>

namespace Pakal
{
	class Engine;
	class GameStateSystem;

	class BaseGameState
	{
		friend GameStateSystem;

	public:
		const char* get_name()
		{
			return m_name.c_str();
		}

	private:
		GameStateSystem* game_state_system;
		bool deallocate_on_pop;
		std::string m_name;

	protected:

		BaseGameState(const std::string& name): game_state_system(nullptr), deallocate_on_pop(false)
		{
			m_name = name;
		}

		~BaseGameState() { }

		const GameStateSystem* get_game_state_system()
		{
			return game_state_system;
		};

		virtual void initialize(Engine* engine) = 0;

		virtual void terminate() {};

		virtual void onResume() {};
		virtual void onPause() {};
	};
}