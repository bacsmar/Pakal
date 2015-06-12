#pragma once

#include "Config.h"
#include "GameStateSystem.h"
#include "BaseGameState.h"

using namespace Pakal;

GameStateSystem::~GameStateSystem() 
{
	while (!states.empty())
	{
		BaseGameState* state = states.top();
		states.pop();

		state->terminate();
		if (state->deallocate_on_pop) SAFE_DEL(state);
	}
}

void GameStateSystem::initialize(Engine* engine)
{
	m_engine = engine;
}

void GameStateSystem::transition_to_state(BaseGameState* new_state, bool deallocate_on_pop)
{
	BaseGameState* state = peek_state();

	new_state->game_state_system = this;
	new_state->deallocate_on_pop = deallocate_on_pop;
	new_state->initialize(m_engine);
	
	states.pop();
	states.push(new_state);

	state->terminate();
	if (state->deallocate_on_pop) SAFE_DEL(state);

}

void GameStateSystem::push_state(BaseGameState* new_state, bool deallocate_on_pop)
{
	if (!states.empty())
	{
		states.top()->onPause();
	}

	new_state->game_state_system = this;
	new_state->deallocate_on_pop = deallocate_on_pop;
	new_state->initialize(m_engine);
	
	states.push(new_state);
}

void GameStateSystem::pop_state()
{
	ASSERT(!states.empty());

	BaseGameState* state;
	
	state = peek_state();
	state->terminate();
	states.pop();

	if (state->deallocate_on_pop) SAFE_DEL(state);
	if (states.empty())  return;
	
	state = peek_state();
	state->onResume();
}

void GameStateSystem::pause_state()
{
	ASSERT(!states.empty());

	BaseGameState* state = peek_state();

	state->onPause();
}

void GameStateSystem::resume_state()
{
	ASSERT(!states.empty());

	BaseGameState* state = peek_state();

	state->onResume();
}

void GameStateSystem::pop_states(int amount)
{
	ASSERT(size() >= amount);

	for (int i = 0; i < amount; i++)
	{
		pop_state();
	}

}

BaseGameState* GameStateSystem::peek_state()
{
	return states.top();
}

int GameStateSystem::size() const
{
	return states.size();
}