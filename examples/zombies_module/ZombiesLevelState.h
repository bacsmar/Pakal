///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ZombiesLevelState.h
// Purpose: Plays one converted Zombies level: loads level.json (see tools/zombies_convert), builds the
//          physics, the actors and their models, runs the level scripts, and draws the camera and
//          the HUD. The gameplay itself is ZombiesGame (actors and components).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "AssetCache.h"
#include "BaseGameState.h"
#include "Components.h"
#include "EventArgs.h"
#include "TopDown.h"
#include "topdown/CameraPan.h"
#include "topdown/view/CameraRig.h"
#include "topdown/view/CharacterViewComponent.h"
#include "topdown/view/InputMapper.h"
#include "topdown/view/LevelEditPanel.h"
#include "topdown/view/OccluderFade.h"
#include "topdown/view/SceneLook.h"
#include "GameRules.h"
#include "LevelDocument.h"
#include "Meta.h"
#include "topdown/MouseAim.h"
#include "RandomStreams.h"
#include "SimulationClock.h"
#include "ZombieScript.h"
#include "ZombiesGame.h"
#include "ZombiesHud.h"
#include "bgfx/BgfxGraphicsSystem.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <random>
#include <mutex>
#include <string>
#include <vector>

namespace Pakal
{
	class BgfxGraphicsSystem;
	class IUIManager;
	class CameraComponent3D_Bgfx;
	class IInputManager;
	class MeshComponent3D_Bgfx;
	class NavGrid2D;
	class PhysicsWorld2D;
}

namespace Pakal::TopDown
{
	class AudioCues;
	class EffectsSystem;
	class SpawnSystem;
}

namespace Zombies
{
	// The level format is the engine's (tools/level_doc)
	using Pakal::LevelDocument;
	using Pakal::LevelObject;
	using Json = Pakal::Json;

	// What a level hands to the next one it changes to (a restart, the next mission): its asset cache and its models, so the
	// files both use are not loaded again
	struct LevelCarry
	{
		std::shared_ptr<Pakal::AssetCache> cache;
		std::map<std::string, Pakal::AssetHandle<Pakal::MeshComponent3D_Bgfx>> models;
		// a reload while the level is edited keeps the player where it was, and the editor as it was
		struct Edit
		{
			bool keep = false;
			b2Vec2 playerAt = b2Vec2(0.0f, 0.0f);
			b2Vec2 focus = b2Vec2(0.0f, 0.0f);   // the editing camera's
			bool editing = false;
			bool liveReload = false;
			std::string selected;
		} edit;
	};

	class ZombiesLevelState : public Pakal::BaseGameState
	{
	public:
		// A mission of the campaign ('missionId') plays with the player's loadout and pays its reward; without it, the level as it is
		explicit ZombiesLevelState(const std::string& levelPath, const std::string& missionId = std::string(), LevelCarry carried = LevelCarry());
		~ZombiesLevelState() override;

	protected:
		void on_initialize(Pakal::Engine* engine) override;
		void on_terminate(Pakal::Engine* engine) override;
		void on_update(unsigned long dtMilliseconds) override;
		void on_activate(Pakal::Engine* engine) override;
		void on_deactivate(Pakal::Engine* engine) override;

	private:
		// Loading (ZombiesLoading.cpp), in steps across frames so the loading screen keeps drawing (each step runs on the graphics thread: models
		// are made there): the document and the game, then the level's model files a few per frame, then the rest
		bool load_begin();
		bool load_models(double budgetMs);   // true once every model file is loaded
		bool load_finish();
		void continue_loading();
		void loading_done();
		void show_loading_progress(float progress);
		std::unique_ptr<LevelDocument> m_loadDoc;
		std::vector<std::string> m_modelQueue;
		size_t m_modelsLoaded = 0;
		int m_loadStage = 0;
		bool m_loadFailed = false;
		std::chrono::steady_clock::time_point m_loadStart;
		void build_physics(const LevelDocument& doc);
		void build_objects(const LevelDocument& doc);
		void build_player(const LevelDocument& doc);
		uintptr_t spawn_enemy(const std::string& type, const b2Vec2& at);
		// Models: one loaded prototype per file (from the asset cache), shared by every object and character that uses it
		std::unique_ptr<Pakal::MeshComponent3D_Bgfx> create_instance(const std::string& file);

		// The level's scripts (ZombiesScripting.cpp): the original game's script functions on this level, and the objectives
		// run in the scene's order
		void start_objective_scripts(const LevelDocument& doc);
		void begin_objective(size_t index);
		void run_deactivate_hook();
		void update_script(float dt);
		void show_object(int id, bool shown);
		void enable_object(int id, bool enabled);
		void warp_player(int id);
		void start_camera_pan(int fromId, const std::string& fromOffset, int toId, const std::string& toOffset, float seconds);

		// Each fixed step
		void simulate(float dt);

		// Each frame
		void update_views(float dt);
		void update_camera();
		void update_fade();
		void draw_gameplay_lines();
		void draw_hud();
		void play_sound(const std::string& name, const b2Vec2& at);


		std::string m_levelPath;
		std::string m_levelName;
		Pakal::Engine* m_engine = nullptr;
		// Down: the player can buy a revive (its price doubles each time in the same level), paid with the money picked up
		// first and then the profile's
		int m_revives = 0;
		float m_gloss = 0.0f;   // the plastic toy look's highlight
		static constexpr float kGlossShininess = Pakal::TopDown::SceneLook::kGlossShininess;   // ... and how tight it is
		static constexpr float kPlayerRadius = 0.3f;      // the player's body (meters)
		static constexpr unsigned kLoadingDocument = 2;   // the loading screen (an RmlUi document)
		int revive_price() const;
		int money_to_spend() const;
		void revive_player();
		Pakal::BgfxGraphicsSystem* m_graphics = nullptr;
		bool m_loaded = false;

		// Simulation
		GameRules m_rules;
		std::unique_ptr<Pakal::PhysicsWorld2D> m_world;
		std::unique_ptr<Pakal::NavGrid2D> m_nav;
		std::unique_ptr<ZombiesGame> m_game;
		std::unique_ptr<SpawnSystem> m_spawns;
		std::unique_ptr<AudioCues> m_audio;
		std::unique_ptr<EffectsSystem> m_effects;
		uintptr_t m_playerId = 0;
		std::map<int, std::vector<uintptr_t>> m_actorsByGameId; // the game's object ids -> prop actors
		Pakal::RandomStreams m_random;     // the level's seed; the presentation's streams ("shake"); the game keeps its own
		Pakal::SimulationClock m_clock;   // the simulation's fixed steps, and how far each frame is between two

		// Models
		CharacterViewComponent::Look m_playerLook;
		CharacterViewComponent::Look m_enemyLook;                            // default for the enemy types
		std::map<std::string, CharacterViewComponent::Look> m_enemyLooks;    // by enemy type ("25001")
		std::string m_playerModel;
		std::map<std::string, std::string> m_heroModels;   // the kit's survivors (visuals.heroes): the player picks one, an escort wears another
		std::string m_enemyModel;
		// The models load in the background (AssetCache: decoded on its workers, uploaded a few per frame); the prototypes the
		// placed models share must outlive them
		std::shared_ptr<Pakal::AssetCache> m_assetCache;
		std::map<std::string, Pakal::AssetHandle<Pakal::MeshComponent3D_Bgfx>> m_prototypes;
		LevelCarry m_carried;   // the previous level's, let go once this one is loaded
		std::vector<std::unique_ptr<Pakal::MeshComponent3D_Bgfx>> m_objects;            // placed level models
		// Tall buildings fade out while they stand between the camera and the player
		OccluderFade m_occluders;
		void update_occluders(float dt);
		// Free zombie models, by model file: the types of enemy can look different
		std::map<std::string, std::vector<std::unique_ptr<Pakal::MeshComponent3D_Bgfx>>> m_enemyPools;
		static constexpr int kEnemyPoolSize = 28; // zombie models made at load, unless the level says ("enemyPool")
		std::string enemy_model_key(const CharacterViewComponent::Look& look) const;
		// Every model a zombie of this look can wear: its model and its variants (the level's enemy model when it names none)
		std::vector<std::string> enemy_model_keys(const CharacterViewComponent::Look& look) const;

		// Level scripts (ZombieScript)
		std::unique_ptr<ZombieScript> m_script;
		Json m_objectiveHooks;                     // objective id -> { onActivate, run, onDeactivate } (text)
		std::vector<std::string> m_objectiveOrder; // the scene runs the objectives in this order
		size_t m_objectiveIndex = 0;
		std::string m_objectiveRunCode;            // the objective's Run hook, started once OnActivate has finished
		bool m_runStarted = false;
		bool m_objectiveDone = false;              // Done() or DoneKillAll() of the current objective
		bool m_levelComplete = false;
		bool m_levelFailed = false;                // Fail(): lost (the timer ran out, what had to be protected fell)
		std::vector<int> m_markers;                // ShowMarker: the points with a beacon
		bool point_position(int id, b2Vec2& at) const; // a named point or a camera anchor, in meters
		std::string m_failText;              // DoneKillAll
		std::string m_objectiveKey;                // SetObjectiveText; empty after RemoveObjectiveText
		double m_timerEndMs = -1.0;                // StartTimer: game time the countdown reaches 0 (< 0: no countdown)
		std::string m_dialog;                      // ShowAutoDialog text, shown until m_dialogUntilMs
		double m_dialogUntilMs = 0.0;
		std::map<int, b2Vec2> m_warpPoints;        // named points of the level by game id

		// Camera
		std::unique_ptr<Pakal::CameraComponent3D_Bgfx> m_camera;
		std::map<int, CameraPan::Point> m_cameraAnchors;
		std::unique_ptr<CameraRig> m_rig;   // follows the player, pans, shakes
		// FadeOut / FadeIn of the scripts: the screen's cover goes from one amount to another over a time (game clock)
		struct ScreenFade
		{
			float color[3] = { 0.0f, 0.0f, 0.0f };
			float from = 0.0f, to = 0.0f;
			double startMs = 0.0, durationMs = 0.0;
		} m_fade;
		float m_lastPlayerHealth = -1.0f;
		double m_hitFlashMs = -1.0e9;   // game time the player was last hit (the red flash of the screen)

		// Input: the keys, the mouse and the sticks (InputMapper)
		Pakal::IInputManager* m_input = nullptr;
		std::unique_ptr<InputMapper> m_controls;
		bool m_paused = false;
		// The HUD and the pause/down/complete panel (ZombiesHud); what its buttons do is here
		HudView hud_view() const;
		ZombiesHud m_hud;
		Pakal::IUIManager* m_ui = nullptr;
		bool m_pauseEscHeld = true;
		bool m_pausedByDebug = false;
		void take_requested_screenshot();
		void update_locked(unsigned long dtMilliseconds);
		std::recursive_mutex m_stateMutex;   // the game's update and the development panel (another thread on Android)
		std::string m_transitionTo;          // the level to change to once the update ends
		std::string m_transitionMission;     // ... as this mission
		bool m_transitionMenu = false;       // ... or back to the menu
		// The campaign's mission this level is (empty: a level started on its own)
		std::string m_missionId;
		bool m_missionSettled = false;
		MissionReward m_reward;
		void settle_mission(bool won);
		void handle_hud_click(const std::string& element);
		void use_boost(const std::string& id);
		// Development panel (Dear ImGui, F1, ZombiesDevPanel.cpp): what it asks for is done at the next update, outside the
		// drawing
		void dev_panel();
		unsigned m_devPanelId = 0;
		std::string m_devLevelRequest;      // load this level
		bool m_devRestartRequest = false;
		// Editing the running level (the dev panel's "Edit level", ZOMBIES_EDIT=1) and reloading it when its file changes
		// (ZOMBIES_LIVE_RELOAD=1: pakal-level, an agent)
		std::unique_ptr<LevelEditPanel> m_edit;
		std::vector<Pakal::MeshComponent3D_Bgfx*> m_objectMeshes;   // by the document's object index (null: none)
		std::filesystem::file_time_type m_levelWriteTime{};
		float m_liveCheck = 0.0f;
		bool m_wasEditing = false;
		b2Vec2 m_editFocus = b2Vec2(0.0f, 0.0f);   // what the camera looks at while editing (WASD moves it)
		LevelCarry::Edit m_carryEdit;
		void update_editing();
		bool m_devGodMode = false;          // the player takes no damage
		uint32_t m_devGodModifier = 0;      // ... a modifier of its defence (Stats)
		bool m_noFade = false;              // nothing fades (ZOMBIES_NO_FADE)
		// The level's own picture settings, and what the dev panel turns off of them (apply_dev_graphics)
		Pakal::BgfxGraphicsSystem::PostSettings m_levelPost;
		Pakal::BgfxGraphicsSystem::ShadowSettings m_levelShadows;
		struct DevGraphics { bool post = true, ao = true, tilt = true, shadows = true; } m_devGraphics;
		void apply_dev_graphics();
		std::vector<std::string> m_devLevels; // the levels found next to this one
		int m_devLevelIndex = -1;
		bool m_pauseKeyHeld = false;
		bool m_restartKeyHeld = true; // release R before another restart can be requested
		bool m_showColliders = false;
		bool m_colliderKeyHeld = false;

		// Debug and checks (environment variables, see docs)
		float m_reportTimer = 0.0f;
		float m_elapsedSeconds = 0.0f;
		bool m_screenshotTaken = false;
		int m_shotsTaken = 0;
		double m_deadForMs = 0.0;
		float m_debugSinceKill = 0.0f;     // ZOMBIES_DEBUG_KILL_EVERY
		bool m_debugBroke = false;         // ZOMBIES_DEBUG_BREAK
		std::vector<std::pair<double, std::string>> m_scriptedClicks; // ZOMBIES_HUD_CLICKS still to come
		bool m_scriptedClicksRead = false;
	};
}
