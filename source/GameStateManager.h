#pragma once
#include <stack>
#include "Config.h"
#include "IManager.h"

namespace Pakal
{
	class BaseGameState;
	class Engine;

	class _PAKALExport GameStateManager final : IManager
	{
		std::stack<BaseGameState*> states;
		Engine* m_engine;

	public:

		explicit GameStateManager(Engine* engine) : m_engine(engine) {};
		virtual ~GameStateManager();

		void initialize() override {};
		void terminate() override;
		void update();
	
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