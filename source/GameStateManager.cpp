#pragma once

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

		state->terminate();
		if (state->m_deallocate_on_pop) SAFE_DEL(state);
	}
}

void GameStateManager::transition_to_state(BaseGameState* new_state, bool deallocate_on_pop)
{
	BaseGameState* state = peek_state();

	new_state->m_game_state_manager = this;
	new_state->m_deallocate_on_pop = deallocate_on_pop;
	new_state->initialize(m_engine);
	
	states.pop();
	states.push(new_state);

	state->terminate();
	if (state->m_deallocate_on_pop) SAFE_DEL(state);

}

void GameStateManager::push_state(BaseGameState* new_state, bool deallocate_on_pop)
{
	if (!states.empty())
	{
		states.top()->on_pause();
	}

	new_state->m_game_state_manager = this;
	new_state->m_deallocate_on_pop = deallocate_on_pop;
	new_state->initialize(m_engine);
	
	states.push(new_state);
}

void GameStateManager::pop_state()
{
	ASSERT(!states.empty());

	BaseGameState* state;
	
	state = peek_state();
	state->terminate();
	states.pop();

	if (state->m_deallocate_on_pop) SAFE_DEL(state);
	if (states.empty())  return;
	
	state = peek_state();
	state->on_resume();
}

void GameStateManager::pause_state()
{
	ASSERT(!states.empty());

	BaseGameState* state = peek_state();

	state->on_pause();
}

void GameStateManager::resume_state()
{
	ASSERT(!states.empty());

	BaseGameState* state = peek_state();

	state->on_resume();
}

void GameStateManager::pop_states(int amount)
{
	ASSERT(size() >= amount);

	for (int i = 0; i < amount; i++)
	{
		pop_state();
	}

}

BaseGameState* GameStateManager::peek_state()
{
	return states.top();
}

int GameStateManager::size() const
{
	return states.size();
}