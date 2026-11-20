#pragma once
#include <atomic>
#include <mutex>
#include <stack>
#include <string>
#include <vector>
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
		// What the stack holds, as the QA tools' "state" reads it: each state's name and status line (BaseGameState::describe),
		// top first, and the game's own line (IPakalApplication::describe)
		struct Snapshot
		{
			struct Entry { std::string name, status; };
			std::vector<Entry> states;
			std::string game;
			unsigned long long generation = 0;   // how many snapshots were taken before this one
		};

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

		// From any thread: asks for a new snapshot (taken on the game's thread at its next update) and waits for it up to
		// 'waitMs'; returns the newest there is (the last one, when the game does not update: paused, or loading)
		Snapshot snapshot(int waitMs);

	private:
		void take_snapshot();
		std::mutex m_snapshotMutex;
		Snapshot m_snapshot;
		std::atomic<bool> m_snapshotWanted{ false };
	};


}
