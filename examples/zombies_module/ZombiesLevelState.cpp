///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ZombiesLevelState.cpp
// Purpose: Plays one converted Zombies level - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ZombiesLevelState.h"
#include "topdown/ActorOrientation.h"
#include "topdown/view/AudioCues.h"
#include "topdown/CameraInput.h"
#include "AutoplayBot.h"
#include "WorldOverlays.h"
#include "ZombiesHud.h"
#include "topdown/view/RenderSpace.h"
#include "Components.h"
#include "topdown/kits/shooter/WeaponComponent.h"
#include "topdown/view/EffectsSystem.h"
#include "topdown/view/PropMesh.h"
#include "topdown/LevelPhysics.h"
#include "topdown/SpawnSystem.h"
#include "MenuState.h"

#include "Engine.h"
#include "EventArgs.h"
#include "GameStateManager.h"
#include "IInputManager.h"
#include "LogMgr.h"
#include "Profiler.h"
#include "OSManager.h"
#include "IUIManager.h"
#include "bgfx/BgfxGraphicsSystem.h"
#include "bgfx/CameraComponent3D_Bgfx.h"
#include "bgfx/MeshComponent3D_Bgfx.h"
#include "box2D/PhysicsWorld2D.h"
#include "gameplay/NavGrid2D.h"

#include <dear-imgui/imgui.h>
#include <filesystem>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <cstdlib>
#include <set>
#include <sstream>

using namespace Pakal;

namespace Zombies
{
	namespace
	{

		const float kReportEvery = 2.0f;
		const int kRevivePrice = 250;             // a revive, doubled each time in the same level
		const float kReviveInvincibleSeconds = 3.0f;
		const float kReviveClearRadius = 5.0f;    // the zombies this close are thrown back
		const float kCameraHeight = 17.1f; // original GameSettings CamPositionOffset (-1250, -1250, 1710) cm, Z up
		const float kCameraBack = 12.5f;  // render +Z = level south
		const float kCameraSide = -12.5f; // level west
		const float kCameraFov = 22.0f;
		// CameraObject::ShakeCamera: a shake from a point is full within 4 m of the camera and nothing past 50 m; it lasts
		// 800 ms times that share and jitters up to power x 0.7 x share centimeters, fading out over its last quarter
		const float kShakeCloseCm = 400.0f, kShakeFarCm = 5000.0f, kShakeCloseMs = 800.0f, kShakeClosePower = 0.7f;
		const float kShakeFade = 0.25f;
		// ... and the screen flashes red (the HUD's GetHit overlay in the original)
		const float kHitFlashMs = 350.0f, kHitFlashStrength = 0.22f;
		const float kStickAimMinimum = 0.2f;    // aim stick: below this the player keeps facing where it walks
		const float kStickFireThreshold = 0.41f; // aim stick: past this it fires
		const float kAimPlaneHeight = 0.9f; // the mouse aims at chest height, where the bodies are
		// WASD/IJKL are relative to the view: the camera sits southwest of the player and looks
		// northeast, so "up" on screen is northeast in level coords (see CameraInput.h)
		const float kCameraRelativeInputAngle = camera_input_angle(kCameraSide, kCameraBack);


		float env_number(const char* name, float fallback)
		{
			const char* text = std::getenv(name);
			return text && *text ? static_cast<float>(std::atof(text)) : fallback;
		}
	}

	ZombiesLevelState::ZombiesLevelState(const std::string& levelPath, const std::string& missionId, LevelCarry carried) :
		BaseGameState("ZombiesLevel"),
		m_levelPath(levelPath),
		m_carried(std::move(carried)),
		m_missionId(missionId)
	{
	}

	ZombiesLevelState::~ZombiesLevelState() = default;

	void ZombiesLevelState::on_initialize(Engine* engine)
	{
		m_engine = engine;
		m_graphics = static_cast<BgfxGraphicsSystem*>(engine->graphics_system());
		Session::get().apply_options_once(engine);   // the saved options (quality, FSR), when no menu came first
		if (!m_graphics)
		{
			LOG_ERROR("[ZombiesLevel] No graphics system");
			return;
		}
		m_input = engine->os_manager() ? engine->os_manager()->get_input_manager() : nullptr;
		InputMapper::Settings controls;
		controls.cameraAngle = kCameraRelativeInputAngle;
		controls.stickAimMinimum = kStickAimMinimum;
		controls.stickFireThreshold = kStickFireThreshold;
		controls.aimPlaneHeight = kAimPlaneHeight;
		// While the bot plays (ZOMBIES_AUTOPLAY) the keys do nothing: someone typing while its window has the focus would
		// restart or pause the run
		controls.ignoreKeys = std::getenv("ZOMBIES_AUTOPLAY") != nullptr;
		m_controls = std::make_unique<InputMapper>(controls);
		// One seed for the level (ZOMBIES_SEED): the game's streams and the presentation's (shake, effects, sounds)
		m_random = Pakal::RandomStreams(static_cast<uint32_t>(env_number("ZOMBIES_SEED", 2026.0f)));
		// The original's camera (GameSettings CamPositionOffset, CameraObject::ShakeCamera)
		CameraRig::Settings camera;
		camera.side = kCameraSide;
		camera.height = kCameraHeight;
		camera.back = kCameraBack;
		camera.fovY = kCameraFov;
		camera.closeCm = kShakeCloseCm;
		camera.farCm = kShakeFarCm;
		camera.closeMs = kShakeCloseMs;
		camera.closePower = kShakeClosePower;
		camera.fade = kShakeFade;
		m_rig = std::make_unique<CameraRig>(camera, m_random["shake"]);
		m_audio = std::make_unique<AudioCues>(engine->sound_manager(), LevelDocument::directory_of(m_levelPath),
			Pakal::RandomStreams::seed_for(m_random.seed(), "audio"));

		// The level loads in steps from the updates (continue_loading), under the loading screen
		m_ui = engine->get_ui_manager();
		if (m_ui && m_ui->load_document(kLoadingDocument, "Assets/ui/loading.rml"))
		{
			m_ui->set_text(kLoadingDocument, "loading_level", std::filesystem::path(m_levelPath).parent_path().filename().string());
			// Our own tips (not the original game's)
			static const char* tips[] = {
				"<em>Shotguns</em> knock zombies back: make room when they crowd you.",
				"Shoot the <em>fire hydrants</em> and the <em>cars</em>: they break, and some zombies get caught.",
				"The <em>gift box</em> holds a weapon or a boost. Open it when the street is quiet.",
				"<em>Boosts</em> make you glow: speed, damage or defence for a while.",
				"Zombies that are far away walk faster to catch up. Keep moving.",
				"Pick up the <em>money</em> the zombies drop before it fades."
			};
			std::mt19937 pick(static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()));
			m_ui->set_markup(kLoadingDocument, "loading_tip", tips[pick() % (sizeof(tips) / sizeof(tips[0]))]);
			m_ui->show_document(kLoadingDocument);
		}
		m_loadStart = std::chrono::steady_clock::now();
		m_noFade = std::getenv("ZOMBIES_NO_FADE") != nullptr;
	}

	// One step of the loading per update (this thread waits for it; the graphics thread draws the loading screen between steps)
	void ZombiesLevelState::handle_hud_click(const std::string& element)
	{
		if (Session::get().handle_option(element, m_engine))
		{
			m_hud.options_changed();   // filled again with the new choice
			return;
		}
		if (element == "pause_button") m_paused = !m_paused;
		else if (element == "btn_resume") m_paused = false;
		else if (element == "btn_revive") revive_player();
		else if (element == "btn_restart") m_devRestartRequest = true;
		else if (element == "btn_menu")
		{
			m_transitionMenu = true;
		}
		else if (element == "btn_next" && !m_missionId.empty())
		{
			const Session& session = Session::get();
			const int index = session.campaign.mission_index(m_missionId) + 1;
			if (index > 0 && index < static_cast<int>(session.campaign.missions.size()) && session.profile.unlocked(session.campaign, static_cast<size_t>(index)))
			{
				const MissionDef& next = session.campaign.missions[static_cast<size_t>(index)];
				m_transitionTo = Session::resolve(next.level);
				m_transitionMission = next.id;
			}
		}
		else if (element.rfind("useboost_", 0) == 0) use_boost(element.substr(9));
	}

	// A boost the player carries: used now, one less in the profile
	void ZombiesLevelState::use_boost(const std::string& id)
	{
		Session& session = Session::get();
		const BoostDef* boost = session.campaign.boost(id);
		Actor* player = m_game ? m_game->find(m_playerId) : nullptr;
		const HealthComponent* health = player ? player->get<HealthComponent>() : nullptr;
		if (!boost || !player || !health || health->is_dead() || m_paused || session.profile.boost_count(id) <= 0) return;
		auto rules = m_rules.consumables.find(boost->consumable);
		if (rules == m_rules.consumables.end()) return;
		player->get<ConsumableComponent>()->apply(*m_game, rules->second);
		--session.profile.boosts[id];
		session.save();
		m_hud.refresh_boosts(m_missionId);
	}

	// What the mission gives: the money picked up always, the reward and the stars for a win. Once per run.
	void ZombiesLevelState::settle_mission(bool won)
	{
		if (m_missionSettled || m_missionId.empty() || !m_game) return;
		m_missionSettled = true;
		Session& session = Session::get();
		MissionResult result;
		result.won = won;
		result.kills = m_game->kills;
		result.moneyPicked = m_game->money;
		result.seconds = static_cast<float>(m_game->timeMs / 1000.0);
		if (const Actor* player = m_game->find(m_playerId))
		{
			if (const HealthComponent* health = player->get<HealthComponent>()) result.healthShare = health->current() / std::max(1.0f, health->maximum());
		}
		m_reward = settle(session.campaign, session.profile, m_missionId, result);
		session.save();
		LOG_INFO("[ZombiesLevel] mission %s %s: %d stars, reward %d, picked %d, money now %d", m_missionId.c_str(), won ? "won" : "lost",
			m_reward.starCount, m_reward.reward, m_reward.moneyPicked, m_reward.moneyAfter);
	}

	int ZombiesLevelState::revive_price() const
	{
		return kRevivePrice << std::min(m_revives, 6);
	}

	int ZombiesLevelState::money_to_spend() const
	{
		return m_game->money + (m_missionId.empty() ? 0 : Session::get().profile.money);
	}

	// Back up: all the health, a moment of invincibility, and the zombies around thrown back. Paid with the money picked up
	// in the level first, then the profile's (the revive screen of the original, which sold it).
	void ZombiesLevelState::revive_player()
	{
		Actor* player = m_game ? m_game->find(m_playerId) : nullptr;
		HealthComponent* health = player ? player->get<HealthComponent>() : nullptr;
		const int price = revive_price();
		if (!health || !health->is_dead() || m_levelFailed || money_to_spend() < price) return;
		const int fromLevel = std::min(price, m_game->money);
		m_game->money -= fromLevel;
		if (price > fromLevel)
		{
			Session& session = Session::get();
			session.profile.money -= price - fromLevel;
			session.save();
		}
		++m_revives;
		health->revive(kReviveInvincibleSeconds);
		m_deadForMs = 0.0;
		for (const auto& actor : m_game->actors())
		{
			EnemyBrainComponent* brain = actor->get<EnemyBrainComponent>();
			if (brain && !brain->is_dead() && (actor->position - player->position).Length() < kReviveClearRadius)
			{
				brain->damage(*m_game, DamageInfo{ .from = player->position, .knockback = true });
			}
		}
		m_game->events.emit(ShockwaveEvent{ player->position, kReviveClearRadius, m_rules.shake.enemyBlast });
		m_game->play(m_rules.sound("revive"), player->position);
		LOG_INFO("[ZombiesLevel] revived for $%d (%d so far)", price, m_revives);
	}

	// Level models; the ones the game refers to by id are prop actors (scripts show and hide them,
	// bullets break them, the gift box opens)
	void ZombiesLevelState::on_activate(Engine* engine)
	{
		(void)engine;
		LOG_INFO("[ZombiesLevel] level active");
	}

	void ZombiesLevelState::on_deactivate(Engine* engine)
	{
		(void)engine;
	}

	// The development panel runs on the graphics thread (on Android it is not the game's): the level's state is locked
	// while it updates. Changing the level destroys this state, so it happens after the lock is released.
	void ZombiesLevelState::on_update(unsigned long dtMilliseconds)
	{
		std::string next, mission;
		bool menu = false;
		LevelCarry carry;
		{
			std::lock_guard<std::recursive_mutex> lock(m_stateMutex);
			update_locked(dtMilliseconds);
			next.swap(m_transitionTo);
			mission = m_transitionMission;
			menu = m_transitionMenu;
			if (!next.empty() && !menu)
			{
				// the next level keeps this one's models until its own are loaded (a restart loads almost nothing)
				carry.cache = m_assetCache;
				carry.models = std::move(m_prototypes);
				carry.edit = m_carryEdit;
			}
		}
		// transition_to_state destroys this instance. Do not access members afterward.
		if (menu)
		{
			get_manager()->transition_to_state(new MenuState(MenuState::Screen::Missions), true);
		}
		else if (!next.empty())
		{
			get_manager()->transition_to_state(new ZombiesLevelState(next, mission, std::move(carry)), true);
		}
	}

	void ZombiesLevelState::update_locked(unsigned long dtMilliseconds)
	{
		if (!m_loaded)
		{
			continue_loading();
			return;
		}
		{
			std::vector<std::string> clicks = m_hud.take_clicks();
			// Debug: ZOMBIES_HUD_CLICKS=<seconds>:<id>,... clicks those HUD elements at those game times (the revive, the options)
			if (!m_scriptedClicksRead)
			{
				m_scriptedClicksRead = true;
				if (const char* text = std::getenv("ZOMBIES_HUD_CLICKS"))
				{
					std::stringstream list(text);
					for (std::string item; std::getline(list, item, ',');)
					{
						const size_t colon = item.find(':');
						if (colon != std::string::npos) m_scriptedClicks.emplace_back(std::atof(item.substr(0, colon).c_str()), item.substr(colon + 1));
					}
				}
			}
			while (!m_scriptedClicks.empty() && m_game->timeMs / 1000.0 >= m_scriptedClicks.front().first)
			{
				clicks.push_back(m_scriptedClicks.front().second);
				m_scriptedClicks.erase(m_scriptedClicks.begin());
			}
			for (const std::string& element : clicks) handle_hud_click(element);
			if (m_transitionMenu || !m_transitionTo.empty()) return;
		}
		if (m_levelComplete) settle_mission(true);
		if (m_levelFailed) settle_mission(false);
		update_editing();
		if (!m_transitionTo.empty()) return;
		if (!m_devLevelRequest.empty())
		{
			LOG_INFO("[ZombiesLevel] loading level %s (development panel)", m_devLevelRequest.c_str());
			m_transitionTo = m_devLevelRequest;
			m_transitionMission.clear();
			return;
		}
		const bool restart = m_controls->key_down(Key::R) || m_devRestartRequest;
		if (restart && !m_restartKeyHeld)
		{
			LOG_INFO("[ZombiesLevel] restarting level");
			m_transitionTo = m_levelPath;
			m_transitionMission = m_missionId;
			return;
		}
		m_restartKeyHeld = restart;
		const bool pause = m_controls->key_down(Key::P);
		if (pause && !m_pauseKeyHeld) m_paused = !m_paused;
		m_pauseKeyHeld = pause;
		const bool escape = m_controls->key_down(Key::Escape);
		if (escape && !m_pauseEscHeld) m_paused = !m_paused;
		m_pauseEscHeld = escape;
		if (m_paused || m_levelFailed)
		{
			// Keep drawing, but freeze the scripts, physics and timers (also once the mission is lost). The real seconds go on
			// (debug captures count them); the game does not catch up the paused time.
			m_elapsedSeconds += m_clock.frame_seconds();
			m_clock.hold();
			draw_gameplay_lines();
			draw_hud();
			take_requested_screenshot();
			return;
		}
		// The simulation runs in fixed steps (SimulationClock). Loading can take several seconds: the first frame brings no
		// time, so that is not played as gameplay. The clock reads the time itself (the engine's dt is in whole milliseconds).
		(void)dtMilliseconds;
		// Debug: ZOMBIES_TIMESCALE=8 plays eight times faster (the bot's runs). The same steps run, only more of them per
		// frame, so the game is the same (at most a quarter of a second of game time per frame: SimulationClock).
		static const float timeScale = std::max(0.0f, env_number("ZOMBIES_TIMESCALE", 1.0f));
		const float frameSeconds = m_clock.frame_seconds() * timeScale;
		for (int steps = m_clock.advance(frameSeconds); steps > 0; --steps)
		{
			simulate(m_clock.step());
		}
		// The models are drawn between the last step and the next (m_clock.alpha()), so they move smoothly whatever the
		// frame rate
		update_views(frameSeconds);
		draw_hud();

		// Debug: ZOMBIES_SHOTS=<prefix>,<first second>,<interval>,<count> saves a series of frames
		static const char* series = std::getenv("ZOMBIES_SHOTS");
		if (series && m_shotsTaken < 1000)
		{
			char prefix[260];
			float first = 0.0f, interval = 0.1f;
			int count = 0;
			if (std::sscanf(series, "%259[^,],%f,%f,%d", prefix, &first, &interval, &count) == 4 && m_shotsTaken < count
				&& m_elapsedSeconds + frameSeconds >= first + interval * static_cast<float>(m_shotsTaken))
			{
				const std::string path = std::string(prefix) + std::to_string(m_shotsTaken) + ".tga";
				++m_shotsTaken;
				m_graphics->execute_block([this, path]() { m_graphics->request_screenshot(path.c_str()); });
			}
		}

		m_elapsedSeconds += frameSeconds;
		// Debug: ZOMBIES_PAUSE_AT=<s> pauses the game at that time (to look at the pause panel)
		static const float pauseAt = env_number("ZOMBIES_PAUSE_AT", -1.0f);
		if (pauseAt >= 0.0f && m_elapsedSeconds >= pauseAt && !m_pausedByDebug)
		{
			m_pausedByDebug = true;
			m_paused = true;
		}
		take_requested_screenshot();
	}

	// Optional frame capture for checking the scene (ZOMBIES_SCREENSHOT=<file.tga>, at ZOMBIES_SCREENSHOT_AT seconds, 3 by default)
	void ZombiesLevelState::take_requested_screenshot()
	{
		const char* shot = std::getenv("ZOMBIES_SCREENSHOT");
		if (shot && *shot && !m_screenshotTaken && m_elapsedSeconds >= env_number("ZOMBIES_SCREENSHOT_AT", 3.0f))
		{
			m_screenshotTaken = true;
			const std::string path = shot;
			m_graphics->execute_block([this, path]() { m_graphics->request_screenshot(path.c_str()); });
		}
	}

	void ZombiesLevelState::simulate(float dt)
	{
		PAKAL_PROFILE_SCOPE("sim.step");
		const bool colliderKey = m_controls->key_down(Key::F3);
		if (colliderKey && !m_colliderKeyHeld) m_showColliders = !m_showColliders;
		m_colliderKeyHeld = colliderKey;

		Actor* player = m_game->find(m_playerId);
		const HealthComponent* health = player ? player->get<HealthComponent>() : nullptr;
		const bool dead = !health || health->is_dead();
		if (dead)
		{
			m_deadForMs += dt * 1000.0;
		}
		const bool playing = !dead && !m_levelComplete;

		// Debug: ZOMBIES_DEBUG_KILL_EVERY=<seconds> removes one living zombie at that interval
		static const float killEvery = env_number("ZOMBIES_DEBUG_KILL_EVERY", 0.0f);
		if (playing && killEvery > 0.0f && (m_debugSinceKill += dt) >= killEvery)
		{
			m_debugSinceKill = 0.0f;
			for (const auto& actor : m_game->actors())
			{
				EnemyBrainComponent* brain = actor->get<EnemyBrainComponent>();
				if (brain && !brain->is_dead()) { brain->kill(*m_game); break; }
			}
		}
		// Debug: ZOMBIES_DEBUG_BREAK=<meters> breaks every prop that close to the player 3 s into the game (their effects)
		static const float breakAround = env_number("ZOMBIES_DEBUG_BREAK", 0.0f);
		if (playing && breakAround > 0.0f && !m_debugBroke && m_game->timeMs > 3000.0 && m_game->player())
		{
			m_debugBroke = true;
			std::vector<Actor*> near;
			for (const auto& actor : m_game->actors())
			{
				if (actor->get<DestructibleComponent>() && (actor->position - m_game->player()->position).Length() < breakAround) near.push_back(actor.get());
			}
			for (Actor* actor : near) m_game->damage(*actor, 1.0e6f);
		}

		if (playing)
		{
			update_script(dt);
			if (!m_game->frozen()) m_spawns->update(*m_game, dt); // nothing comes while the player is down
		}
		PlayerInput input;
		if (playing)
		{
			static const bool autoplay = std::getenv("ZOMBIES_AUTOPLAY") != nullptr;
			input = autoplay ? autoplay_input(*m_game, m_playerId)
				: m_controls->read(*m_rig, static_cast<float>(m_graphics->get_width()), static_cast<float>(m_graphics->get_height()));
			// Debug: ZOMBIES_AUTOMOVE=x,y holds that movement (level axes: +x east, +y north)
			static const char* autoMove = std::getenv("ZOMBIES_AUTOMOVE");
			if (autoMove)
			{
				std::sscanf(autoMove, "%f,%f", &input.move.x, &input.move.y);
			}
			if (std::getenv("ZOMBIES_AUTOFIRE")) input.fire = true;
		}
		// The floor the camera shows (the box around its four corners): zombies far off it walk faster and the ones left
		// behind are brought ahead of the player. The camera here is where it sits at this step (over the player's position,
		// without the shake), not the drawn one: that one lies between steps and depends on the frame rate, and the game must
		// not.
		if (m_graphics && !m_rig->panning())
		{
			const float width = static_cast<float>(m_graphics->get_width()), height = static_cast<float>(m_graphics->get_height());
			const Actor* focus = m_game->find(m_playerId);
			const tmath::vector3df p = floor_point(focus ? focus->position : b2Vec2(0.0f, 0.0f));
			static const float scale = env_number("ZOMBIES_CAM_SCALE", 1.0f);
			Vec3 eye, target;
			m_rig->rest_pose(Vec3{ p.x, p.y, p.z }, scale, eye, target);
			b2Vec2 low(1e9f, 1e9f), high(-1e9f, -1e9f);
			int corners = 0;
			for (const float px : { 0.0f, width - 1.0f })
			{
				for (const float py : { 0.0f, height - 1.0f })
				{
					float x = 0.0f, y = 0.0f;
					if (mouse_on_plane(eye, target, m_rig->settings().fovY, px, py, width, height, 0.0f, x, y))
					{
						low = b2Vec2(std::min(low.x, x), std::min(low.y, y));
						high = b2Vec2(std::max(high.x, x), std::max(high.y, y));
						++corners;
					}
				}
			}
			if (corners == 4) m_game->set_screen(low, high);
		}
		m_game->step(dt, input);
		// The player hit: the camera shakes (HealthComponent)
		if (const Actor* hero = m_game->player())
		{
			if (const HealthComponent* health = hero->get<HealthComponent>())
			{
				if (m_lastPlayerHealth >= 0.0f && health->current() < m_lastPlayerHealth - 0.0001f)
				{
					m_rig->shake_from(Vec3{ hero->position.x, 0.0f, -hero->position.y }, m_rules.shake.hurt);
					m_hitFlashMs = m_game->timeMs;
				}
				m_lastPlayerHealth = health->current();
			}
		}

		m_reportTimer += dt;
		if (m_reportTimer >= kReportEvery && player)
		{
			m_reportTimer = 0.0f;
			const WeaponComponent* weapon = player->get<WeaponComponent>();
			LOG_INFO("[ZombiesLevel] t %.1f s, A %.0f, kills %d, alive %d, health %.1f, $%d, ammo %d/%d, at %.2f %.2f, spawns: %s",
				m_game->timeMs / 1000.0, m_script ? m_script->get_variable("A") : 0.0, m_game->kills, m_game->alive_enemies(),
				health ? health->current() : 0.0f, m_game->money, weapon ? weapon->magazine() : 0, weapon ? weapon->rounds() : 0,
				player->position.x, player->position.y, m_spawns->describe().c_str());
		}
	}

	void ZombiesLevelState::play_sound(const std::string& name, const b2Vec2& at)
	{
		if (!m_audio)
		{
			return;
		}
		// Louder near the player, silent off screen
		const Actor* player = m_game ? m_game->find(m_playerId) : nullptr;
		const float distance = player ? (at - player->position).Length() : 0.0f;
		const float volume = std::clamp(1.0f - (distance - 6.0f) / 14.0f, 0.0f, 1.0f);
		m_audio->play(name, volume);
	}

	void ZombiesLevelState::update_views(float dt)
	{
		PAKAL_PROFILE_SCOPE("level.views");
		if (Pakal::Profiler::get().enabled())
		{
			Pakal::Profiler::get().counter("game.actors", static_cast<double>(m_game->actors().size()));
			Pakal::Profiler::get().counter("game.enemies", m_game->alive_enemies());
		}
		draw_gameplay_lines();
		static const char* clipTest = std::getenv("ZOMBIES_CLIP_TEST"); // debug: every character plays this clip
		for (const auto& actor : m_game->actors())
		{
			if (CharacterViewComponent* view = actor->get<CharacterViewComponent>())
			{
				if (clipTest) view->force_clip(clipTest);
				view->freeze(m_game->frozen() && actor.get() != m_game->player());
				view->sync(dt, m_clock.alpha());
			}
		}
		if (m_effects)
		{
			m_effects->update(dt, m_game->bullets());
		}
		update_camera();
		update_fade();
		update_occluders(dt);
	}

	void ZombiesLevelState::update_fade()
	{
		if (!m_graphics)
		{
			return;
		}
		const double elapsed = m_game->timeMs - m_fade.startMs;
		const float t = m_fade.durationMs <= 0.0 ? 1.0f : static_cast<float>(std::min(1.0, std::max(0.0, elapsed / m_fade.durationMs)));
		const float amount = m_fade.from + (m_fade.to - m_fade.from) * t;
		// A hit flashes the screen red for a moment, unless the script's fade covers more of it
		const float hit = kHitFlashStrength * (1.0f - static_cast<float>(std::min(1.0, (m_game->timeMs - m_hitFlashMs) / kHitFlashMs)));
		if (hit > amount)
		{
			m_graphics->set_screen_fade(0.75f, 0.0f, 0.0f, hit);
			return;
		}
		m_graphics->set_screen_fade(m_fade.color[0], m_fade.color[1], m_fade.color[2], amount);
	}

	// A building between the camera and the player fades out so the player stays in view
	void ZombiesLevelState::update_occluders(float dt)
	{
		if (m_occluders.empty() || m_noFade)
		{
			return;
		}
		const Actor* player = m_game ? m_game->find(m_playerId) : nullptr;
		const Vec3 head{ player ? player->position.x : 0.0f, 1.3f, player ? -player->position.y : 0.0f };
		m_occluders.update(m_rig->eye(), head, player && !m_rig->panning(), dt);
	}

	// The camera follows the player from above and behind, or flies a scripted pan (CameraRig)
	void ZombiesLevelState::update_camera()
	{
		if (!m_camera)
		{
			return;
		}
		const Actor* focus = m_game->find(m_playerId);
		if (m_edit && m_edit->editing())
		{
			// editing: the camera goes where WASD takes it
			const tmath::vector3df at = floor_point(m_editFocus);
			m_rig->update(Vec3{ at.x, at.y, at.z }, m_game->timeMs, 1.0f);
			m_rig->apply(*m_camera, static_cast<int>(m_graphics->get_width()), static_cast<int>(m_graphics->get_height()));
			return;
		}
		if (const char* onEnemy = std::getenv("ZOMBIES_CAM_ON_ENEMY")) // debug: look at the first zombie alive (of the type given, if a number)
		{
			const int wanted = std::atoi(onEnemy);
			for (const auto& actor : m_game->actors())
			{
				if (m_game->faction_of(*actor) == m_game->undead && !actor->removed && (wanted <= 1 || actor->gameId == wanted)) { focus = actor.get(); break; }
			}
		}
		// drawn between simulation steps, so it moves smoothly
		tmath::vector3df p = floor_point(focus ? focus->drawn_position(m_clock.alpha()) : b2Vec2(0.0f, 0.0f));
		static const char* camAt = std::getenv("ZOMBIES_CAM_AT"); // debug: look at a point "x,y" (meters) instead
		if (camAt)
		{
			float ax = 0.0f, ay = 0.0f;
			std::sscanf(camAt, "%f,%f", &ax, &ay);
			p = floor_point(b2Vec2(ax, ay));
		}
		static const float scale = env_number("ZOMBIES_CAM_SCALE", 1.0f); // debug: pull the camera back
		m_rig->update(Vec3{ p.x, p.y, p.z }, m_game->timeMs, scale);
		m_rig->apply(*m_camera, static_cast<int>(m_graphics->get_width()), static_cast<int>(m_graphics->get_height()));
	}

	void ZombiesLevelState::draw_gameplay_lines()
	{
		OverlayScene scene{ *m_graphics, *m_game, m_rules, *m_rig, *m_controls, *m_world, m_markers };
		scene.playerId = m_playerId;
		scene.alpha = m_clock.alpha();
		scene.pointPosition = [this](int id, b2Vec2& at) { return point_position(id, at); };
		scene.showColliders = m_showColliders;
		draw_world_overlays(scene);
	}

	// What the HUD shows this frame
	HudView ZombiesLevelState::hud_view() const
	{
		HudView view;
		view.levelName = m_levelName;
		const Actor* player = m_game->find(m_playerId);
		const HealthComponent* health = player ? player->get<HealthComponent>() : nullptr;
		const WeaponComponent* weapon = player ? player->get<WeaponComponent>() : nullptr;
		const ConsumableComponent* consumable = player ? player->get<ConsumableComponent>() : nullptr;
		const PlayerControlComponent* control = player ? player->get<PlayerControlComponent>() : nullptr;
		view.health = health ? health->current() : 0.0f;
		view.healthMax = health ? health->maximum() : 0.0f;
		view.dead = !health || health->is_dead();
		view.down = view.dead && m_deadForMs >= m_rules.player.delayAfterDeathMs;
		view.money = m_game->money;
		if (consumable && consumable->melee()) view.meleeName = consumable->active()->name;
		if (weapon)
		{
			view.hasWeapon = true;
			view.gunName = weapon->gun().name;
			view.magazine = weapon->magazine();
			view.rounds = weapon->rounds();
			view.reloading = weapon->reloading();
		}
		view.kills = m_game->kills;
		view.killGoal = m_script ? static_cast<int>(m_script->get_variable("killCountRequired")) : 0;
		view.intro = control && !control->enabled;
		view.berserk = m_game->berserk;
		view.objectiveKey = m_objectiveKey;
		view.timer = m_timerEndMs >= 0.0 && !view.dead && !m_levelComplete;
		if (view.timer) view.timerSeconds = static_cast<int>(std::ceil(std::max(0.0, m_timerEndMs - m_game->timeMs) / 1000.0));
		view.boost = consumable && consumable->is_active();
		if (view.boost)
		{
			view.boostName = consumable->active()->name;
			view.boostSecondsLeft = consumable->seconds_left();
		}
		if (!m_dialog.empty() && m_game->timeMs < m_dialogUntilMs) view.dialog = m_dialog;
		view.paused = m_paused && !(m_edit && m_edit->editing());   // editing waits without the pause menu
		view.complete = m_levelComplete;
		view.failed = m_levelFailed;
		view.failText = m_failText;
		view.mission = !m_missionId.empty();
		view.missionSettled = m_missionSettled;
		view.reward = m_reward;
		if (!m_reward.heroRescued.empty())
		{
			const HeroDef* hero = Session::get().campaign.hero(m_reward.heroRescued);
			view.heroRescuedName = hero ? hero->title : m_reward.heroRescued;
		}
		view.revivePrice = revive_price();
		view.moneyToSpend = money_to_spend();
		if (view.mission)
		{
			const Session& session = Session::get();
			const int index = session.campaign.mission_index(m_missionId) + 1;
			view.hasNext = index > 0 && index < static_cast<int>(session.campaign.missions.size()) && session.profile.unlocked(session.campaign, static_cast<size_t>(index));
		}
		return view;
	}

	void ZombiesLevelState::draw_hud()
	{
		if (m_graphics && m_game)
		{
			m_hud.draw(*m_graphics, hud_view());
		}
	}

	void ZombiesLevelState::on_terminate(Engine* engine)
	{
		(void)engine;
		if (m_input) m_input->set_touch_controls(false);
		if (m_controls) m_controls->stop();
		if (!m_graphics)
		{
			return;
		}
		if (m_devPanelId) m_graphics->get_debug_ui().remove_panel(m_devPanelId);
		{
			std::lock_guard<std::recursive_mutex> lock(m_stateMutex);
			settle_mission(m_levelComplete);
		}
		m_hud.unload();
		if (m_ui) m_ui->unload_document(kLoadingDocument); // left mid-load
		// Models and the camera release bgfx resources on the graphics thread; the game goes first, so its
		// zombie models return to the pool before the pool is emptied
		auto task = m_graphics->execute_block([this]()
		{
			m_graphics->clear_active_camera(m_camera.get());
			m_graphics->set_post_process(BgfxGraphicsSystem::PostSettings());
			m_graphics->set_shadows(BgfxGraphicsSystem::ShadowSettings());
			m_graphics->clear_gi_volume();
			m_graphics->clear_lightmap();
			m_spawns.reset();
			if (m_effects) m_effects->release();
			m_effects.reset();
			m_game.reset();
			m_enemyPools.clear();
			m_objects.clear();
			m_prototypes.clear(); // after every instance that shares their buffers
			m_camera.reset();
		});
		if (task)
		{
			task->wait();
		}
		m_script.reset();
		m_nav.reset();
		m_world.reset();
		m_audio.reset();
		m_loaded = false;
	}
}
