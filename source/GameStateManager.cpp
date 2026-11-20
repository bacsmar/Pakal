#include "Config.h"
#include "GameStateManager.h"
#include "BaseGameState.h"
#include "Engine.h"
#include "IPakalApplication.h"

#include <chrono>
#include <thread>

using namespace Pakal;

GameStateManager::~GameStateManager() 
{
	GameStateManager::terminate();
	m_engine = nullptr;
}

void GameStateManager::terminate()
{
	while (!states.empty())
	{
		BaseGameState* state = states.top();
		states.pop();
		
		state->on_terminate(m_engine);

		if (state->m_deallocate_on_pop) 
			SAFE_DEL(state);
	}
}

void GameStateManager::update(unsigned long dtMilliseconds)
{
	for (int asked = m_popRequests.exchange(0); asked > 0 && !states.empty(); --asked)
		pop_state();
	if (!states.empty())
		peek_state()->on_update(dtMilliseconds);
	if (m_snapshotWanted.exchange(false))
		take_snapshot();
}

void GameStateManager::take_snapshot()
{
	Snapshot taken;
	for (std::stack<BaseGameState*> copy = states; !copy.empty(); copy.pop())
		taken.states.push_back({ copy.top()->get_name(), copy.top()->describe() });
	if (m_engine && m_engine->get_application())
		taken.game = m_engine->get_application()->describe();
	std::lock_guard<std::mutex> lock(m_snapshotMutex);
	taken.generation = m_snapshot.generation + 1;
	m_snapshot = std::move(taken);
}

GameStateManager::Snapshot GameStateManager::snapshot(int waitMs)
{
	unsigned long long before;
	{
		std::lock_guard<std::mutex> lock(m_snapshotMutex);
		before = m_snapshot.generation;
	}
	m_snapshotWanted.store(true);
	const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(waitMs);
	while (std::chrono::steady_clock::now() < until)
	{
		{
			std::lock_guard<std::mutex> lock(m_snapshotMutex);
			if (m_snapshot.generation != before) return m_snapshot;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	std::lock_guard<std::mutex> lock(m_snapshotMutex);
	return m_snapshot;
}

void GameStateManager::transition_to_state(BaseGameState* new_state, bool deallocate_on_pop)
{
	BaseGameState* state = peek_state();

	new_state->m_game_state_manager = this;
	new_state->m_deallocate_on_pop = deallocate_on_pop;

	state->on_terminate(m_engine);
	new_state->on_initialize(m_engine);
	
	states.pop();
	states.push(new_state);

	if (state->m_deallocate_on_pop) 
		SAFE_DEL(state);

}

void GameStateManager::push_state(BaseGameState* new_state, bool deallocate_on_pop)
{
	if (!states.empty())
	{
		states.top()->on_deactivate(m_engine);
	}

	new_state->m_game_state_manager = this;
	new_state->m_deallocate_on_pop = deallocate_on_pop;
	new_state->on_initialize(m_engine);
	
	states.push(new_state);
}

void GameStateManager::pop_state()
{
	ASSERT(!states.empty());

	BaseGameState* state;
	
	state = peek_state();
	state->on_terminate(m_engine);
	states.pop();

	if (state->m_deallocate_on_pop) 
		SAFE_DEL(state);

	if (states.empty())  
		return;
	
	state = peek_state();
	state->on_activate(m_engine);
}

void GameStateManager::pop_states(int amount)
{
	ASSERT(size() >= amount);

	for (int i = 0; i < amount; i++)
	{
		pop_state();
	}

}

void GameStateManager::pause()
{
	peek_state()->on_deactivate(m_engine);
}

void GameStateManager::resume()
{
	peek_state()->on_activate(m_engine);
}

BaseGameState* GameStateManager::peek_state()
{
	return states.top();
}

int GameStateManager::size() const
{
	return states.size();
}