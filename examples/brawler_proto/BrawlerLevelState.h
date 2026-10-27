///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: BrawlerLevelState.h
// Purpose: Plays the brawler prototype: the arena of brawler.json on TopDown's LevelRuntime (camera,
//          input, models, look, fixed steps), a camera from the front that follows the hero along the
//          arena and stops at its ends, the buttons (J light, K heavy, L dodge, Space jump, R again) and
//          its HUD (an RmlUi document: everyone's health, the hits in a row, the result).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "BaseGameState.h"
#include "BrawlerGame.h"
#include "topdown/view/LevelRuntime.h"
#include "topdown/view/UiPanel.h"

#include <atomic>
#include <memory>
#include <string>

namespace Pakal
{
	class Engine;
	class NavGrid2D;
	class PhysicsWorld2D;
}

namespace Brawler
{
	class BrawlerLevelState : public Pakal::BaseGameState
	{
	public:
		BrawlerLevelState();
		~BrawlerLevelState() override;

	protected:
		void on_initialize(Pakal::Engine* engine) override;
		void on_terminate(Pakal::Engine* engine) override;
		void on_update(unsigned long dtMilliseconds) override;
		void on_activate(Pakal::Engine* engine) override;
		void on_deactivate(Pakal::Engine* engine) override;

	private:
		bool load();
		void update_camera();
		void update_hud();

		Pakal::Engine* m_engine = nullptr;
		BrawlerRules m_rules;
		std::unique_ptr<Pakal::PhysicsWorld2D> m_world;
		std::unique_ptr<Pakal::NavGrid2D> m_nav;
		std::unique_ptr<BrawlerGame> m_game;
		std::unique_ptr<LevelRuntime> m_runtime;
		std::unique_ptr<UiPanel> m_hud;
		bool m_loadTried = false;
		bool m_loaded = false;
		bool m_autoplay = false;
		bool m_restartHeld = false;
		std::atomic<int> m_impacts{ 0 };   // clip marks "impact" seen (graphics thread)
		int m_impactsShaken = 0;
		int m_shownHits = 0;
		bool m_reported = false;
		double m_seconds = 0.0;            // real seconds since the start (screenshots)
		bool m_shot = false;
	};
}
