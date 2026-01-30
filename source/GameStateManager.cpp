#include "Config.h"
#include "GameStateManager.h"
#include "BaseGameState.h"

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
	if (states.empty())
		return;
	peek_state()->on_update(dtMilliseconds);
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