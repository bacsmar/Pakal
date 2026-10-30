#pragma once
#include <atomic>
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
		std::atomic<int> m_popRequests{ 0 };

	public:

		explicit GameStateManager(Engine* engine) : m_engine(engine) {};
		virtual ~GameStateManager();

		void initialize() override {};
		void terminate() override;
		void update(unsigned long dtMilliseconds);
	
		void transition_to_state(BaseGameState* newState, bool deallocateOnPop = true);
		void push_state(BaseGameState* newState, bool deallocateOnPop = true);
		void pop_state();
		void pop_states(int amount);
		// From any thread (a tool's panel on the graphics thread): the state on top leaves at the start of the next update
		void request_pop() { ++m_popRequests; }

		void pause();
		void resume();

		BaseGameState* peek_state();
		int size() const;
	};

	
}