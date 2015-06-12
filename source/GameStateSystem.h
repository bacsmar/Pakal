#pragma once
#include <stack>


namespace Pakal
{
	class BaseGameState;
	class Engine;

	class GameStateSystem
	{
	private:
		std::stack<BaseGameState*> states;
		Engine* m_engine;

	public:

		GameStateSystem() {};
		~GameStateSystem();

		void initialize(Engine* engine);
	
		void transition_to_state(BaseGameState* new_state, bool deallocate_on_pop = true);
		void push_state(BaseGameState* new_state, bool deallocate_on_pop = true);
		void pop_state();
		void pause_state();
		void resume_state();
		void pop_states(int amount);

		BaseGameState* peek_state();
		int size() const;
	};

	
}