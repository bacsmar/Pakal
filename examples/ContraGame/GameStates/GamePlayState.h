///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: GamePlayState.h
// Purpose: Main gameplay state for Contra game
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "BaseGameState.h"
#include <vector>
#include <memory>
#include <string>

namespace Pakal
{
	class GenericEntity;
	class Engine;
	class SceneLoader;
	
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
		// Scene management
		void load_level(int levelNumber);
		void cleanup_level_entities();
		void setup_loaded_scene();
		void setup_entity_components();
		bool is_back_to_menu_pressed() const;

		// Game logic
		void update_game_logic(float deltaTime);
		void check_win_lose_conditions();
		void register_level_entity(GenericEntity* entity);
		
		Engine* m_engine;
		std::unique_ptr<SceneLoader> m_sceneLoader;
		
		GenericEntity* m_player;
		GenericEntity* m_goal;
		GenericEntity* m_camera;
		std::vector<GenericEntity*> m_levelEntities;
		std::vector<GenericEntity*> m_enemies;
		
		int m_currentLevel;
		int m_maxLevels;
		float m_levelTransitionTimer;
		bool m_levelCompleted;
		bool m_returningToMenu;

		int m_score;
		int m_lives;
		int m_enemiesKilled;
		int m_totalEnemies;
		
		bool m_gameWon;
		bool m_gameLost;
	};
}
