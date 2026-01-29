///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: GamePlayState.h
// Creation Date: January 2026
//
// Purpose: Main gameplay state
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "BaseGameState.h"
#include "PakalMath.h"
#include <vector>

namespace Pakal
{
	class Entity;
	class CollisionSystem;
	class SpawnSystem;
	class ScoreSystem;

	class _PAKALExport GamePlayState : public BaseGameState
	{
	public:
		GamePlayState();
		~GamePlayState() override;

	protected:
		void on_initialize(Engine* engine) override;
		void on_terminate(Engine* engine) override;
		void on_update(unsigned long dtMilliseconds) override;
		void on_activate(Engine* engine) override;
		void on_deactivate(Engine* engine) override;

	private:
		void on_render();
		void create_level();
		void create_player();
		void create_camera();
		void setup_physics();
		void update_camera(float deltaTime);
		void update_ui();
		void check_win_condition();
		void check_lose_condition();

		Entity* create_platform(float x, float y, float width, float height);
		Entity* spawn_enemy_soldier(float x, float y);
		Entity* spawn_enemy_turret(float x, float y);

		Engine* m_engine;

		// Entities
		Entity* m_player;
		Entity* m_camera;
		std::vector<Entity*> m_enemies;
		std::vector<Entity*> m_platforms;

		// Systems
		CollisionSystem* m_collisionSystem;
		SpawnSystem* m_spawnSystem;
		ScoreSystem* m_scoreSystem;

		// Game state
		bool m_isPaused;
		float m_gameTime;

		// Level data
		float m_levelWidth;
		float m_levelHeight;
		tmath::vector2df m_cameraMin;
		tmath::vector2df m_cameraMax;
	};
}
