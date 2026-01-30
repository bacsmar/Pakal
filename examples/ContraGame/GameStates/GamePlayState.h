///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: GamePlayState.h
// Purpose: Main gameplay state for Contra game
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "BaseGameState.h"
#include <vector>

namespace Pakal
{
	class Entity;
	class Engine;
	
	class GamePlayState : public BaseGameState
	{
	public:
		GamePlayState();
		virtual ~GamePlayState();
		
	protected:
		void on_initialize(Engine* engine) override;
		void on_terminate(Engine* engine) override;
		void on_update(unsigned long dtMilliseconds) override;
		void on_activate(Engine* engine) override;
		void on_deactivate(Engine* engine) override;
		
	private:
		void create_level();
		void create_player();
		void create_enemies();
		void create_platform(float x, float y, float width, float height);
		void setup_camera();
		void update_game_logic();
		void check_win_lose_conditions();
		
		Engine* m_engine;
		Entity* m_player;
		Entity* m_camera;
		std::vector<Entity*> m_enemies;
		
		int m_score;
		int m_lives;
		int m_enemiesKilled;
		int m_totalEnemies;
		
		bool m_gameWon;
		bool m_gameLost;
	};
}
