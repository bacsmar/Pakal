///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: BrawlerLevelState.cpp
// Purpose: Plays the brawler prototype - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "BrawlerLevelState.h"

#include "Engine.h"
#include "GameStateManager.h"
#include "IInputManager.h"
#include "IUIManager.h"
#include "LogMgr.h"
#include "bgfx/BgfxGraphicsSystem.h"
#include "box2D/PhysicsWorld2D.h"
#include "gameplay/NavGrid2D.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>

using namespace Pakal;
using namespace Pakal::TopDown;

namespace Brawler
{
	namespace
	{
		const char* kRulesFile = "Assets/brawler/brawler.json";
		const char* kHudFile = "Assets/brawler/ui/hud.rml";
		const unsigned kHudDocument = 10;

		float env_number(const char* name, float fallback)
		{
			const char* value = std::getenv(name);
			return value ? static_cast<float>(std::atof(value)) : fallback;
		}
	}

	BrawlerLevelState::BrawlerLevelState() : BaseGameState("BrawlerLevel")
	{
	}

	BrawlerLevelState::~BrawlerLevelState() = default;

	void BrawlerLevelState::on_initialize(Engine* engine)
	{
		m_engine = engine;
		m_autoplay = std::getenv("BRAWLER_AUTOPLAY") != nullptr;
		// the move stick; no aim stick (the right half has the buttons)
		if (IInputManager* input = engine->input_manager())
		{
			input->set_touch_controls(true);
			input->set_touch_aim(false);
		}
	}

	void BrawlerLevelState::on_activate(Engine* engine)
	{
		(void)engine;
	}

	void BrawlerLevelState::on_deactivate(Engine* engine)
	{
		(void)engine;
	}

	bool BrawlerLevelState::load()
	{
		std::ifstream file(kRulesFile);
		if (!file)
		{
			LOG_ERROR("[Brawler] cannot read %s", kRulesFile);
			return false;
		}
		m_rules = BrawlerRules::from_json(nlohmann::json::parse(file, nullptr, false));
		const uint32_t seed = static_cast<uint32_t>(env_number("BRAWLER_SEED", 2026.0f));
		m_world = std::make_unique<PhysicsWorld2D>(b2Vec2(0.0f, 0.0f));
		m_nav = std::make_unique<NavGrid2D>(0.5f);
		m_game = std::make_unique<BrawlerGame>(*m_world, *m_nav, m_rules, seed);
		m_game->start();

		// From the front, a little above: the arena's depth goes up the screen. J and K are buttons here, not aim keys.
		const nlohmann::json camera = m_rules.visuals.value("camera", nlohmann::json::object());
		LevelRuntime::Settings settings;
		settings.camera.side = camera.value("side", 0.0f);
		settings.camera.height = camera.value("height", 5.5f);
		settings.camera.back = camera.value("back", 10.0f);
		settings.camera.fovY = camera.value("fovY", 30.0f);
		settings.input.ignoreKeys = m_autoplay;
		settings.input.aimKeys = false;
		settings.input.buttons = { { Key::J, Light }, { Key::K, Heavy }, { Key::Space, Jump }, { Key::L, Dodge }, { Key::I, Block } };
		settings.seed = seed;
		m_runtime = std::make_unique<LevelRuntime>(*m_engine);
		if (!m_runtime->begin(settings)) return false;

		// the fighters' models first, so they decode while the scenery is placed
		std::vector<std::string> models;
		for (const auto& [type, fighter] : m_rules.fighters) models.push_back(fighter.look.value("model", std::string()));
		m_runtime->prefetch(models);
		m_runtime->on_graphics([this]()
		{
			SceneLook look = SceneLook::from_json(m_rules.visuals.value("lighting", nlohmann::json::object()), m_rules.visuals.value("post", nlohmann::json::object()));
			m_runtime->apply_look(look);
			if (const char* sketch = std::getenv("BRAWLER_SKETCH")) m_runtime->graphics().set_sketch(static_cast<float>(std::atof(sketch)));
			m_runtime->place_scenery(m_rules.visuals.value("floor", nlohmann::json::array()));
			m_runtime->place_scenery(m_rules.visuals.value("props", nlohmann::json::array()));
			// the fighters, of plasticine (a look's "clay" over the arena's; BRAWLER_CLAY to compare); an "impact" mark of a clip
			// shakes the camera a little
			const nlohmann::json material = m_rules.visuals.value("characters", nlohmann::json::object());
			for (const auto& actor : m_game->actors())
			{
				auto rules = m_rules.fighters.find(actor->name);
				if (rules == m_rules.fighters.end()) continue;
				const nlohmann::json& look = rules->second.look;
				float clay = look.value("clay", material.value("clay", 0.0f));
				if (const char* env = std::getenv("BRAWLER_CLAY")) clay = static_cast<float>(std::atof(env));
				m_runtime->add_view(*actor, look, CharacterViewComponent::Look(), [this](const std::string& mark) { if (mark == "impact") ++m_impacts; },
					clay, look.value("dentsPerMeter", material.value("dentsPerMeter", 9.0f)));
			}
		});
		update_camera();

		// the HUD (the fonts are the Zombies example's, as are the models)
		if (IUIManager* ui = m_engine->get_ui_manager())
		{
			for (const char* font : { "Assets/ui/fonts/Roboto-Regular.ttf", "Assets/ui/fonts/Roboto-Bold.ttf", "Assets/ui/fonts/Bangers-Regular.ttf" }) ui->load_font(font);
			m_hud = std::make_unique<UiPanel>(ui, kHudDocument);
			m_hud->load(kHudFile, "brawler");
		}
		LOG_INFO("[Brawler] arena ready: %zu fighters", m_game->actors().size());
		return true;
	}

	void BrawlerLevelState::on_update(unsigned long dtMilliseconds)
	{
		(void)dtMilliseconds;
		if (!m_loadTried)
		{
			// loaded on the first update: the models are made on the graphics thread, which is running by now
			m_loadTried = true;
			m_loaded = load();
		}
		if (!m_loaded)
		{
			return;
		}
		bool again = false;
		if (m_hud)
		{
			// the buttons of a touch screen hold the game's button a moment (the guard a little longer)
			static const std::pair<const char*, int> touchButtons[] = { { "t_light", Light }, { "t_heavy", Heavy }, { "t_jump", Jump }, { "t_dodge", Dodge }, { "t_guard", Block } };
			for (const auto& button : m_hud->clicks())
			{
				again = again || button == "b_again";
				for (const auto& [id, bit] : touchButtons)
				{
					if (button == id) m_runtime->input().tap(bit, bit == Block ? 30 : 6);
				}
			}
		}
		const bool restart = m_runtime->input().key_down(Key::R);
		if ((restart && !m_restartHeld) || again)
		{
			get_manager()->transition_to_state(new BrawlerLevelState(), true);
			return;   // this state is gone
		}
		m_restartHeld = restart;

		for (int steps = m_runtime->advance(); steps > 0; --steps)
		{
			PlayerInput input;
			if (m_autoplay)
			{
				input = autoplay(*m_game);
			}
			else
			{
				// walking and the buttons: the hero faces the way it walks (no mouse aim) and Space is a jump, not the trigger
				input = m_runtime->read_input();
				input.hasAimPoint = false;
				input.aimDirection = b2Vec2(0.0f, 0.0f);
				input.fire = false;
			}
			m_game->step(m_runtime->step_seconds(), input);
		}
		m_seconds += m_runtime->frame_seconds();

		const int impacts = m_impacts.load();
		if (impacts != m_impactsShaken)
		{
			m_impactsShaken = impacts;
			m_runtime->rig().shake(3.0f, 120.0f);
		}
		m_runtime->sync_views(*m_game);
		update_camera();
		update_hud();

		if (!m_reported && (m_game->won() || m_game->lost()))
		{
			m_reported = true;
			LOG_INFO("[Brawler] %s after %.1f s, hero health %.0f", m_game->won() ? "ROUND CLEAR" : "KO", m_game->timeMs / 1000.0,
				m_game->player() ? m_game->player()->get<HealthComponent>()->current() : 0.0f);
		}
		// BRAWLER_SCREENSHOT=<file>.tga at BRAWLER_SCREENSHOT_AT seconds
		static const char* shot = std::getenv("BRAWLER_SCREENSHOT");
		if (shot && !m_shot && m_seconds >= env_number("BRAWLER_SCREENSHOT_AT", 3.0f))
		{
			m_shot = true;
			const std::string path = shot;
			m_runtime->on_graphics([this, path]() { m_runtime->graphics().request_screenshot(path.c_str()); });
		}
	}

	// It follows the hero across the arena and stops before its ends: the arena's walls stay at the screen's edges
	void BrawlerLevelState::update_camera()
	{
		const nlohmann::json camera = m_rules.visuals.value("camera", nlohmann::json::object());
		const float lock = std::max(0.0f, m_rules.halfWidth - camera.value("lockHalfWidth", 3.5f));
		const Actor* hero = m_game->player();
		static const bool close = std::getenv("BRAWLER_CAM_SCALE") != nullptr;   // close up: on the hero, not the arena
		static const float scale = env_number("BRAWLER_CAM_SCALE", 1.0f);         // debug: below 1 comes closer
		const b2Vec2 at = hero ? hero->drawn_position(m_runtime->alpha()) : b2Vec2(0.0f, 0.0f);
		m_runtime->follow(close ? at : b2Vec2(std::clamp(at.x, -lock, lock), 0.0f), m_game->timeMs, 0.9f, scale);
	}

	void BrawlerLevelState::update_hud()
	{
		if (!m_hud) return;
		const Actor* hero = m_game->player();
		if (const HealthComponent* health = hero ? hero->get<HealthComponent>() : nullptr)
		{
			m_hud->bar("b_hero_fill", health->current() / std::max(1.0f, health->maximum()));
			char text[48];
			std::snprintf(text, sizeof(text), "%.0f / %.0f", health->current(), health->maximum());
			m_hud->text("b_hero_text", text);
		}
		// the enemies, one small panel each
		std::string enemies;
		for (const auto& actor : m_game->actors())
		{
			const HealthComponent* health = actor->get<HealthComponent>();
			if (!health || actor.get() == hero) continue;
			char part[320];
			std::snprintf(part, sizeof(part), "<div class=\"b_panel b_enemy%s\"><div class=\"b_name title\">%s</div><div class=\"b_bar\"><div class=\"b_fill\" style=\"width: %.0f%%;\"></div></div></div>",
				health->is_dead() ? " down" : "", actor->name.c_str(), 100.0f * health->current() / std::max(1.0f, health->maximum()));
			enemies += part;
		}
		m_hud->markup("b_enemies", enemies);
		// the hits so far (every one of the hero's moves that landed)
		const int hits = hero && hero->get<FighterComponent>() ? hero->get<FighterComponent>()->hits() : 0;
		if (hits != m_shownHits)
		{
			m_shownHits = hits;
			m_hud->text("b_hits", std::to_string(hits) + (hits == 1 ? " HIT" : " HITS"));
			m_hud->show("b_hits", hits > 0);
		}
		const bool over = m_game->won() || m_game->lost();
		m_hud->show("b_result", over);
		if (over) m_hud->text("b_result_title", m_game->won() ? "ROUND CLEAR" : "K.O.");
	}

	void BrawlerLevelState::on_terminate(Engine* engine)
	{
		if (IInputManager* input = engine->input_manager()) input->set_touch_aim(true);
		m_hud.reset();
		// the fighters' models go before the prototypes they share
		if (m_runtime) m_runtime->end([this]() { m_game.reset(); });
	}
}
