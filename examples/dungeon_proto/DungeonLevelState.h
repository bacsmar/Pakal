///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: DungeonLevelState.h
// Purpose: Plays the dungeon prototype on TopDown's LevelRuntime: the dungeon of the seed built from the
//          Medieval Village pieces (a floor tile per cell, a wall per open side, walls between the camera
//          and the hero fade out), the characters' models, the loot on the floor, the fireballs and their
//          bursts, a camera from above that follows the hero, the buttons (mouse: walk with WASD, aim and
//          strike; 1-4 or Q E F: the abilities; I: the inventory; R: a new dungeon) and the HUD (an RmlUi
//          document: health and mana orbs, experience, the abilities' cooldowns, the inventory).
//          Online (joined from the menu, or DUNGEON_CONNECT=host[:port]) it is a client of a DungeonServer: the
//          dungeon of the server's seed, its characters drawn from the snapshots (docs/NETWORK.md). A player who
//          hosts runs that server on a thread of the game (DungeonHost) and joins it.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "BaseGameState.h"
#include "DungeonGame.h"
#include "DungeonNet.h"
#include "topdown/net/NetClient.h"
#include "topdown/net/ReplicaGame.h"
#include "topdown/view/LevelRuntime.h"
#include "topdown/view/OccluderFade.h"
#include "topdown/view/UiPanel.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Pakal
{
	class Engine;
	class MeshComponent3D_Bgfx;
	class NavGrid2D;
	class PhysicsWorld2D;
}

namespace Pakal::TopDown
{
	class EffectsSystem;
}

namespace Dungeon
{
	// Alone, or online: joining a server, or hosting one here
	struct DungeonSession
	{
		enum class Mode { Alone, Join, Host } mode = Mode::Alone;
		std::string host;                // Join: the server's address
		uint16_t port = kDefaultPort;    // Join: its port; Host: the port to listen on
	};

	class DungeonLevelState : public Pakal::BaseGameState
	{
	public:
		using Session = DungeonSession;

		// 'seed': the dungeon's (0: DUNGEON_SEED, or 2026). DUNGEON_CONNECT makes a session alone a join.
		explicit DungeonLevelState(uint32_t seed = 0, Session session = Session());
		~DungeonLevelState() override;

	protected:
		void on_initialize(Pakal::Engine* engine) override;
		void on_terminate(Pakal::Engine* engine) override;
		void on_update(unsigned long dtMilliseconds) override;
		void on_activate(Pakal::Engine* engine) override;
		void on_deactivate(Pakal::Engine* engine) override;

	private:
		bool load();
		bool go_online();
		// the game's buttons pressed on the HUD (a touch screen has no keys); the other clicks are returned
		std::vector<std::string> take_ability_taps();
		void back_to_menu();
		void build_world(uint32_t seed, int heroes, bool monsters);
		void add_character_view(Actor& actor);
		// graphics thread: the floor, the walls and the props of the layout
		void build_dungeon();
		// graphics thread: the ambient light the walls let reach each spot, baked into the light grid (darker by the walls and
		// in the corners), in place of the screen's ambient occlusion
		void bake_occlusion(const SceneLook& look);
		void place(Pakal::MeshComponent3D_Bgfx& mesh, const b2Vec2& at, float yaw);
		void update_online();
		void draw_frame(TopDownGame& world, const Actor* hero);
		void sync_loot(TopDownGame& world);
		void draw_missiles(TopDownGame& world);
		// The blows seen on the models: blood where a character was hurt; online also the bursts and the whirlwinds
		// (the server's events do not travel: the snapshots say enough)
		void react(TopDownGame& world, const Actor* hero);
		// A pool of blood that spreads under each body and stays on the floor
		void draw_pools(TopDownGame& world, float dt);
		void show_message(const std::string& text);
		void update_hud(const OwnState& own, bool won, bool lost, int monstersLeft);
		void update_inventory(const OwnState& own);
		// Wear or take off an item: the hero's own (alone) or a command to the server (online)
		void send_gear(DungeonGame::GearCommand command, const std::string& what);
		// The chests' models: open ones darken and glow a moment
		void sync_chests(TopDownGame& world, float dt);

		Pakal::Engine* m_engine = nullptr;
		uint32_t m_seed = 0;
		Session m_session;
		std::unique_ptr<DungeonHost> m_host;   // hosting: the server, on its own thread
		bool m_lost = false;                   // online: the server went away (or never answered)
		double m_onlineSeconds = 0.0;          // online: since joining
		DungeonRules m_rules;
		std::unique_ptr<Pakal::PhysicsWorld2D> m_world;
		std::unique_ptr<Pakal::NavGrid2D> m_nav;
		std::unique_ptr<DungeonGame> m_game;
		// a client of a DungeonServer (DUNGEON_CONNECT): the server's world, drawn from its snapshots
		std::unique_ptr<Pakal::TopDown::Net::NetClient> m_client;
		std::unique_ptr<Pakal::TopDown::Net::ReplicaGame> m_replica;
		int m_shownLevel = 0;
		std::unique_ptr<LevelRuntime> m_runtime;
		std::unique_ptr<UiPanel> m_hud;
		std::unique_ptr<EffectsSystem> m_effects;
		std::vector<std::unique_ptr<Pakal::MeshComponent3D_Bgfx>> m_pieces;
		std::map<uintptr_t, std::unique_ptr<Pakal::MeshComponent3D_Bgfx>> m_loot;   // by the loot's actor
		OccluderFade m_walls;
		std::map<uintptr_t, int> m_hurtSeen;     // react(): each character's hurt clips so far
		std::map<uintptr_t, int> m_actionSeen;   // online: each character's action serial
		struct Flying { b2Vec2 at; std::string name; };
		std::map<uintptr_t, Flying> m_flying;    // online: the missiles in the air
		struct Pool { std::unique_ptr<Pakal::MeshComponent3D_Bgfx> mesh; b2Vec2 at; float age = 0.0f; float size = 0.3f; };
		std::map<uintptr_t, Pool> m_pools;       // by the body's actor
		struct Chest { std::unique_ptr<Pakal::MeshComponent3D_Bgfx> mesh; bool opened = false; float glow = 0.0f; };
		std::map<uintptr_t, Chest> m_chests;     // by the chest's actor
		OwnState m_own;                          // the hero's as last shown (its bag: what a click on it means)
		bool m_paused = false;                   // the pause panel is open (alone: the game stops; online it goes on)
		bool m_pauseHeld = false;
		DungeonBot m_bot;
		bool m_loadTried = false;
		bool m_loaded = false;
		bool m_autoplay = false;
		bool m_restartHeld = false;
		bool m_inventoryHeld = false;
		bool m_inventoryOpen = false;
		std::string m_shownInventory;
		int m_shownMessages = 0;
		float m_messageSeconds = 0.0f;
		bool m_reported = false;
		double m_seconds = 0.0;   // real seconds since the start (screenshots)
		bool m_shot = false;
	};
}
