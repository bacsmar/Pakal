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
	class GenericEntity;
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
		struct LevelConfig
		{
			int levelNumber;
			float startX;
			float goalX;
			int enemyCount;
		};

		void load_level(int levelNumber);
		void cleanup_level_entities();
		void register_level_entity(GenericEntity* entity);
		LevelConfig get_level_config(int levelNumber) const;
		bool is_back_to_menu_pressed() const;

		void create_level();
		void create_player();
		void create_enemies();
		void create_platform(float x, float y, float width, float height);
		void setup_camera();
		void update_game_logic(float deltaTime);
		void check_win_lose_conditions();
		
		Engine* m_engine;
		GenericEntity* m_player;
		GenericEntity* m_camera;
		std::vector<GenericEntity*> m_levelEntities;
		std::vector<GenericEntity*> m_enemies;
		
		int m_currentLevel;
		int m_maxLevels;
		float m_levelTransitionTimer;
		bool m_levelCompleted;
		bool m_returningToMenu;
		float m_playerGoalX;

		int m_score;
		int m_lives;
		int m_enemiesKilled;
		int m_totalEnemies;
		
		bool m_gameWon;
		bool m_gameLost;
	};
}
