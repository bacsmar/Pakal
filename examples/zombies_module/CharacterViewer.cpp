///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CharacterViewer.cpp
// Purpose: The character viewer and the zombie encyclopedia - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "CharacterViewer.h"
#include "LevelLook.h"
#include "LevelDocument.h"
#include "MenuState.h"

#include "Engine.h"
#include "GameStateManager.h"
#include "IUIManager.h"
#include "LogMgr.h"
#include "OSManager.h"
#include "bgfx/BgfxGraphicsSystem.h"
#include "bgfx/CameraComponent3D_Bgfx.h"
#include "bgfx/MeshComponent3D_Bgfx.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

using namespace Pakal;

namespace Zombies
{
	namespace
	{
		const unsigned kViewerDocument = 4;
		const char* kFloorModel = "../downtown/meshes/Street_Asphalt_9x9/Street_Asphalt_9x9.gltf";

		const char* attack_name(EnemyAttack attack)
		{
			switch (attack)
			{
			case EnemyAttack::Melee: return "A swing of its arms.";
			case EnemyAttack::Flash: return "A camera flash that slows you down.";
			case EnemyAttack::Boomer: return "A ground pound, and four shots in a cross.";
			case EnemyAttack::Bomber: return "It blows itself up.";
			case EnemyAttack::DogWalker: return "It lets its dogs loose.";
			case EnemyAttack::Dog: return "It runs, pounces and bites.";
			case EnemyAttack::Constant: return "It windmills its arms: it hurts all the time it is close.";
			case EnemyAttack::Bear: return "It swats on all fours, then stands and swings a chain.";
			}
			return "";
		}

		std::string file_stem(const std::string& path)
		{
			const size_t slash = path.find_last_of("/\\");
			const std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
			return name.substr(0, name.find('.'));
		}

		// Text for the interface's markup
		std::string escape(const std::string& text)
		{
			std::string out;
			for (char c : text)
			{
				if (c == '<') out += "&lt;";
				else if (c == '>') out += "&gt;";
				else if (c == '&') out += "&amp;";
				else out += c;
			}
			return out;
		}

		std::string number(float value)
		{
			char text[32];
			std::snprintf(text, sizeof(text), "%.3g", value);
			return text;
		}

		// The world position of a bone (column-major transform: its last column)
		tmath::vector3df position_of(const float m[16]) { return tmath::vector3df(m[12], m[13], m[14]); }
	}

	CharacterViewerState::CharacterViewerState(Mode mode) : BaseGameState("CharacterViewer"), m_mode(mode)
	{
	}

	CharacterViewerState::~CharacterViewerState() = default;

	void CharacterViewerState::on_initialize(Engine* engine)
	{
		m_engine = engine;
		m_graphics = static_cast<BgfxGraphicsSystem*>(engine->graphics_system());
		m_keys = std::make_unique<TopDown::InputMapper>(TopDown::InputMapper::Settings());
		m_keys->listen(engine->os_manager() ? engine->os_manager()->get_input_manager() : nullptr, engine->os_manager(), [](int, int) { return false; });
		// ZOMBIES_VIEWER_CLICKS=v_next,v_clip_next,...: buttons pressed one every half second (to try it without hands)
		if (const char* clicks = std::getenv("ZOMBIES_VIEWER_CLICKS"))
		{
			std::stringstream list(clicks);
			for (std::string id; std::getline(list, id, ',');) if (!id.empty()) m_scripted.push_back(id);
		}
		if (const char* folder = std::getenv("ZOMBIES_VIEWER_SHOTS"))
		{
			m_shots.folder = folder;
			std::stringstream list(std::getenv("ZOMBIES_VIEWER_SHOT_CLIPS") ? std::getenv("ZOMBIES_VIEWER_SHOT_CLIPS") : "Idle_Loop");
			for (std::string clip; std::getline(list, clip, ',');) if (!clip.empty()) m_shots.clips.push_back(clip);
			m_spin = false;
		}
		// ZOMBIES_VIEWER_YAW=<radians>: where the camera looks from (0: the front; -1.57 the character's right side), still
		if (const char* yaw = std::getenv("ZOMBIES_VIEWER_YAW"))
		{
			m_yaw = static_cast<float>(std::atof(yaw));
			m_spin = false;
		}
		m_ui = engine->get_ui_manager();
		if (m_ui && m_ui->load_document(kViewerDocument, "Assets/ui/viewer.rml"))
		{
#if defined(__ANDROID__)
			m_ui->set_class(kViewerDocument, "viewer", "touch", true);
#endif
			m_ui->show_document(kViewerDocument);
			m_clicks = m_ui->add_click_listener(kViewerDocument, [this](const std::string& element)
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_pending.push_back(element);
			});
		}
		else
		{
			LOG_ERROR("[Viewer] Assets/ui/viewer.rml did not load");
			m_ui = nullptr;
		}
	}

	void CharacterViewerState::on_activate(Engine* engine)
	{
		(void)engine;
	}

	void CharacterViewerState::on_deactivate(Engine* engine)
	{
		(void)engine;
	}

	// The heroes of the campaign and the enemies of its first level, as that level draws them
	bool CharacterViewerState::load()
	{
		Session& session = Session::get();
		std::string level = session.campaign.missions.empty() ? std::string() : session.campaign.missions.front().level;
		if (const char* env = std::getenv("ZOMBIES_VIEWER_LEVEL")) level = env;
		level = Session::resolve(level);
		LevelDocument doc;
		std::string error;
		if (level.empty() || !doc.load(level, error))
		{
			LOG_ERROR("[Viewer] cannot read %s: %s", level.c_str(), error.c_str());
			return false;
		}
		m_levelDir = LevelDocument::directory_of(level);
		m_rules.read(doc.extra.value("rules", Json::object()), doc.extra.value("destructibles", Json::array()));
		const Json visuals = doc.extra.value("visuals", Json::object());

		if (m_mode == Mode::Heroes)
		{
			const Look look = Look::from_json(visuals.value("player", Json()), default_player_look());
			const Json heroes = visuals.value("heroes", Json::object());
			for (const HeroDef& hero : session.campaign.heroes)
			{
				if (!heroes.contains(hero.id)) continue;
				Entry entry;
				entry.id = hero.id;
				entry.title = hero.title;
				entry.text = hero.brief;
				entry.models = { heroes[hero.id].get<std::string>() };
				entry.look = look;
				m_entries.push_back(entry);
			}
			// what a hero can hold: nothing, the gun, or a weapon a consumable lends (by its name in the rules)
			m_weapons.push_back({ "Empty hands" });
			if (look.hasHeld) m_weapons.push_back({ "Gun (" + file_stem(look.held.mesh) + ")", true, look.held });
			for (const auto& [id, held] : look.weapons)
			{
				auto consumable = m_rules.consumables.find(id);
				const std::string name = consumable != m_rules.consumables.end() && !consumable->second.name.empty() ? consumable->second.name : file_stem(held.mesh);
				m_weapons.push_back({ name + (held.hasGrip ? " (2 hands)" : ""), true, held });
			}
		}
		else
		{
			const Json enemies = visuals.value("enemy", Json::object());
			const Look base = Look::from_json(enemies.value("default", Json()), default_enemy_look());
			for (auto it = enemies.begin(); it != enemies.end(); ++it)
			{
				if (it.key() == "default") continue;
				Entry entry;
				entry.id = it.key();
				entry.title = it.key();
				entry.look = Look::from_json(it.value(), base);
				auto rules = m_rules.enemies.find(it.key());
				if (rules != m_rules.enemies.end())
				{
					const EnemyRules& e = rules->second;
					if (!e.name.empty()) entry.title = e.name;
					entry.health = e.health;
					entry.speed = e.speed / 100.0f;
					entry.damage = e.attackDamage;
					entry.attack = attack_name(e.attack);
					m_most[0] = std::max(m_most[0], entry.health);
					m_most[1] = std::max(m_most[1], entry.speed);
					m_most[2] = std::max(m_most[2], entry.damage);
				}
				entry.models.push_back(entry.look.model);
				for (const auto& variant : entry.look.variants) entry.models.push_back(variant);
				m_entries.push_back(entry);
			}
			m_weapons.push_back({ "Nothing" });
			m_weapon = 0;
		}
		if (m_entries.empty())
		{
			LOG_ERROR("[Viewer] no characters in %s", level.c_str());
			return false;
		}
		if (const char* env = std::getenv("ZOMBIES_VIEWER_ENTRY")) m_entry = std::min(m_entries.size() - 1, static_cast<size_t>(std::atoi(env)));
		if (const char* env = std::getenv("ZOMBIES_VIEWER_WEAPON")) m_weapon = std::min(m_weapons.size() - 1, static_cast<size_t>(std::atoi(env)));

		const LevelLook look = LevelLook::read(doc.lighting, doc.extra.value("post", Json::object()));
		auto task = m_graphics->execute_block([this, look]()
		{
			look.apply(*m_graphics);
			m_floor = std::make_unique<MeshComponent3D_Bgfx>(m_graphics);
			if (m_floor->load_model(m_levelDir + kFloorModel))
			{
				float low[3], high[3];
				m_floor->get_bounds(low, high);
				m_floor->set_position(-0.5f * (low[0] + high[0]), -high[1], -0.5f * (low[2] + high[2]));
			}
			m_camera = std::make_unique<CameraComponent3D_Bgfx>(m_graphics);
			m_camera->set_perspective(35.0f, 0.05f, 200.0f);
			m_camera->set_viewport(0, 0, static_cast<int>(m_graphics->get_width()), static_cast<int>(m_graphics->get_height()));
			m_graphics->set_active_camera(m_camera.get());
			show_entry();
		});
		if (task) task->wait();
		fill_panels();
		LOG_INFO("[Viewer] %zu %s", m_entries.size(), m_mode == Mode::Heroes ? "heroes" : "enemies");
		return true;
	}

	void CharacterViewerState::show_entry()
	{
		const Entry& entry = m_entries[m_entry];
		m_weaponMesh.reset();
		m_model = std::make_unique<MeshComponent3D_Bgfx>(m_graphics);
		const std::string file = m_levelDir + entry.models[std::min(m_variant, entry.models.size() - 1)];
		if (!m_model->load_model(file))
		{
			LOG_WARNING("[Viewer] model not loaded: %s", file.c_str());
		}
		m_model->set_in_place(true);
		// ZOMBIES_VIEWER_CLIP=<name>: start on that clip
		if (const char* clip = std::getenv("ZOMBIES_VIEWER_CLIP")) m_clip = m_model->find_animation(clip);
		if (m_clip >= m_model->get_animation_count()) m_clip = -1;
		float low[3], high[3];
		m_model->get_bounds(low, high);
		const float height = high[1] - low[1];
		m_model->set_scale(height > 0.001f ? entry.look.height / height : 1.0f);
		m_model->set_position(0.0f, 0.0f, 0.0f);
		m_model->set_color(entry.look.tint[0], entry.look.tint[1], entry.look.tint[2]);
		for (const auto& bone : entry.look.reach) if (!bone.empty()) m_model->watch_node(bone);
		play_clip();
		show_weapon();
	}

	void CharacterViewerState::show_weapon()
	{
		m_weaponMesh.reset();
		const Entry& entry = m_entries[m_entry];
		const Weapon& weapon = m_weapons[m_weapon];
		if (!weapon.hasHeld)
		{
			m_model->set_reach(std::string(), std::string(), std::string(), std::string(), nullptr, 0.0f);
			return;
		}
		m_weaponMesh = std::make_unique<MeshComponent3D_Bgfx>(m_graphics);
		if (!m_weaponMesh->load_model(m_levelDir + weapon.held.mesh))
		{
			LOG_WARNING("[Viewer] weapon not loaded: %s", weapon.held.mesh.c_str());
		}
		m_weaponMesh->attach_to(m_model.get(), weapon.held.bone, weapon.held.placement().data());
		m_model->watch_node(weapon.held.bone);
		float grip[3];
		if (m_reach && weapon.held.grip_in_bone(grip) && !entry.look.reach[0].empty())
		{
			m_model->set_reach(entry.look.reach[0], entry.look.reach[1], entry.look.reach[2], weapon.held.bone, grip, 1.0f);
		}
		else
		{
			m_model->set_reach(std::string(), std::string(), std::string(), std::string(), nullptr, 0.0f);
		}
	}

	void CharacterViewerState::play_clip()
	{
		if (!m_model || m_model->get_animation_count() == 0) return;
		int index = m_clip;
		if (index < 0)
		{
			// the look's rest: with a two-handed gun, the one that holds it in both hands (the game's too)
			const auto& clips = m_entries[m_entry].look.clips;
			const bool twoHanded = m_mode == Mode::Heroes && m_weapon < m_weapons.size() && m_weapons[m_weapon].held.twoHanded;
			for (const auto* list : { twoHanded ? &clips.gunIdle : nullptr, &clips.idle })
			{
				for (size_t i = 0; list && i < list->size() && index < 0; ++i) index = m_model->find_animation((*list)[i]);
			}
		}
		if (index >= 0) m_model->play_animation(index, true);
		m_model->set_animation_speed(m_paused ? 0.0f : m_speed);
	}

	bool CharacterViewerState::pressed(Key key)
	{
		const bool down = m_keys->key_down(key);
		bool& was = m_held[key];
		const bool edge = down && !was;
		was = down;
		return edge;
	}

	// What a button of the panels (or its key) does
	bool CharacterViewerState::act(const std::string& action)
	{
		const size_t count = m_entries.size();
		const int clips = m_model ? m_model->get_animation_count() : 0;
		const bool enemies = m_mode == Mode::Enemies;
		bool entryChanged = false, weaponChanged = false, clipChanged = false;
		if (action == "v_back")
		{
			get_manager()->transition_to_state(new MenuState(enemies ? MenuState::Screen::Main : MenuState::Screen::Heroes), true);
			return false;   // this state is gone
		}
		else if (action == "v_next") { m_entry = (m_entry + 1) % count; m_variant = 0; entryChanged = true; }
		else if (action == "v_prev") { m_entry = (m_entry + count - 1) % count; m_variant = 0; entryChanged = true; }
		else if (action == "v_clip_next" && clips > 0) { m_clip = m_clip + 1 >= clips ? -1 : m_clip + 1; clipChanged = true; }
		else if (action == "v_clip_prev" && clips > 0) { m_clip = m_clip <= -1 ? clips - 1 : m_clip - 1; clipChanged = true; }
		else if (action == "v_second_next" || action == "v_second_prev")
		{
			const bool next = action == "v_second_next";
			if (enemies)
			{
				const size_t models = m_entries[m_entry].models.size();
				m_variant = next ? (m_variant + 1) % models : (m_variant + models - 1) % models;
				entryChanged = true;
			}
			else
			{
				m_weapon = next ? (m_weapon + 1) % m_weapons.size() : (m_weapon + m_weapons.size() - 1) % m_weapons.size();
				weaponChanged = true;
				clipChanged = m_clip < 0;   // the rest may change with the weapon (two-handed guns)
			}
		}
		else if (action == "v_pause") { m_paused = !m_paused; clipChanged = true; }
		else if (action == "v_slower") { m_speed = std::max(0.125f, m_speed * 0.5f); clipChanged = true; }
		else if (action == "v_faster") { m_speed = std::min(2.0f, m_speed * 2.0f); clipChanged = true; }
		else if (action == "v_spin") m_spin = !m_spin;
		else if (action == "v_closer") m_distance = std::max(0.4f, m_distance - 0.4f);
		else if (action == "v_farther") m_distance = std::min(6.0f, m_distance + 0.4f);
		else if (action == "v_reach") { m_reach = !m_reach; weaponChanged = true; }
		else if (action == "v_lines") m_lines = !m_lines;
		else if (action == "v_choose")
		{
			Session& session = Session::get();
			if (choose_hero(session.profile, m_entries[m_entry].id)) session.save();
		}
		else
		{
			return true;
		}
		if (entryChanged || weaponChanged)
		{
			auto task = m_graphics->execute_block([this, entryChanged]() { if (entryChanged) show_entry(); else show_weapon(); });
			if (task) task->wait();
		}
		if (clipChanged && !entryChanged)
		{
			play_clip();
		}
		fill_panels();
		return true;
	}

	void CharacterViewerState::on_update(unsigned long dtMilliseconds)
	{
		const float dt = std::min(0.1f, dtMilliseconds / 1000.0f);
		if (!m_loadTried)
		{
			// loaded on the first update: the models are made on the graphics thread, which is running by now
			m_loadTried = true;
			m_loaded = m_graphics && load();
		}
		if (!m_loaded)
		{
			if (pressed(Key::Escape)) get_manager()->transition_to_state(new MenuState(MenuState::Screen::Main), true);
			return;
		}
		m_seconds += dt;

		// the panels' buttons, the scripted ones, and the keys
		std::vector<std::string> actions;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			actions.swap(m_pending);
		}
		m_scriptClock += dt;
		if (!m_scripted.empty() && m_scriptClock >= 0.5f)
		{
			m_scriptClock = 0.0f;
			actions.push_back(m_scripted.front());
			m_scripted.erase(m_scripted.begin());
		}
		const std::pair<Key, const char*> keys[] = {
			{ Key::Escape, "v_back" }, { Key::Right, "v_next" }, { Key::Left, "v_prev" }, { Key::Down, "v_clip_next" },
			{ Key::Up, "v_clip_prev" }, { Key::E, "v_second_next" }, { Key::Q, "v_second_prev" }, { Key::V, "v_second_next" },
			{ Key::R, "v_reach" }, { Key::L, "v_lines" }, { Key::T, "v_spin" }, { Key::Space, "v_pause" }, { Key::Z, "v_slower" },
			{ Key::X, "v_faster" } };
		for (const auto& [key, action] : keys)
		{
			if (pressed(key)) actions.push_back(action);
		}
		for (const auto& action : actions)
		{
			if (!act(action)) return;   // back to the menu: this state is gone
		}

		update_camera(dt);
		draw_reach();
		fill_reach();
		if (!m_shots.folder.empty()) take_shots(dt);

		// ZOMBIES_SCREENSHOT=<file>.tga at ZOMBIES_SCREENSHOT_AT seconds (as in the levels)
		static const char* shot = std::getenv("ZOMBIES_SCREENSHOT");
		static const char* at = std::getenv("ZOMBIES_SCREENSHOT_AT");
		if (shot && !m_shot && m_seconds >= (at ? std::atof(at) : 2.0))
		{
			m_shot = true;
			const std::string path = shot;
			m_graphics->execute_block([this, path]() { m_graphics->request_screenshot(path.c_str()); });
		}
	}

	// Each weapon with each clip: posed, then a capture (<folder>/w<weapon>_<clip>.tga) and the hand's distance in the log
	void CharacterViewerState::take_shots(float dt)
	{
		if (m_shots.weapon >= m_weapons.size()) return;
		m_shots.wait -= dt;
		if (m_shots.wait > 0.0f) return;
		const std::string& clip = m_shots.clips[m_shots.clip];
		if (!m_shots.posed)
		{
			m_weapon = m_shots.weapon;
			m_clip = m_model ? m_model->find_animation(clip) : -1;
			auto task = m_graphics->execute_block([this]() { show_weapon(); });
			if (task) task->wait();
			play_clip();
			fill_panels();
			m_shots.posed = true;
			m_shots.wait = 0.5f;   // into the clip
			return;
		}
		char path[512];
		std::snprintf(path, sizeof(path), "%s/w%02zu_%s.tga", m_shots.folder.c_str(), m_shots.weapon, clip.c_str());
		const std::string file = path;
		m_graphics->execute_block([this, file]() { m_graphics->request_screenshot(file.c_str()); });
		LOG_INFO("[Viewer] shot w%02zu %s (%s): other hand %.1f cm from the grip", m_shots.weapon, clip.c_str(), m_weapons[m_shots.weapon].name.c_str(), m_handOff);
		m_shots.posed = false;
		m_shots.wait = 0.3f;
		if (++m_shots.clip >= m_shots.clips.size())
		{
			m_shots.clip = 0;
			if (++m_shots.weapon >= m_weapons.size()) LOG_INFO("[Viewer] shots done");
		}
	}

	// Around the character, a little above: A/D turn, W/S closer and farther, SPIN turns it slowly
	void CharacterViewerState::update_camera(float dt)
	{
		if (!m_camera) return;
		if (m_keys->key_down(Key::A)) m_yaw -= 1.8f * dt;
		if (m_keys->key_down(Key::D)) m_yaw += 1.8f * dt;
		if (m_keys->key_down(Key::W)) m_distance = std::max(0.4f, m_distance - 1.2f * dt);
		if (m_keys->key_down(Key::S)) m_distance = std::min(6.0f, m_distance + 1.2f * dt);
		if (m_spin) m_yaw += 0.35f * dt;
		// far enough to see all of it: m_distance is in heights of the character
		const float height = m_entries[m_entry].look.height;
		const float targetY = height * 0.5f;
		const float distance = m_distance * height;
		const float r = distance * std::cos(m_pitch);
		m_camera->set_position(r * std::sin(m_yaw), targetY + distance * std::sin(m_pitch), r * std::cos(m_yaw));
		m_camera->look_at(0.0f, targetY, 0.0f);
	}

	// The reaching arm as lines (shoulder, elbow, hand) and the grip it reaches for; how far the hand is from it
	void CharacterViewerState::draw_reach()
	{
		m_handOff = -1.0f;
		const Entry& entry = m_entries[m_entry];
		const Weapon& weapon = m_weapons[m_weapon];
		float grip[3];
		if (!m_model || !weapon.hasHeld || !weapon.held.grip_in_bone(grip) || entry.look.reach[0].empty())
		{
			return;
		}
		float upper[16], lower[16], hand[16], anchor[16];
		if (!m_model->get_node_world(entry.look.reach[0], upper) || !m_model->get_node_world(entry.look.reach[1], lower)
			|| !m_model->get_node_world(entry.look.reach[2], hand) || !m_model->get_node_world(weapon.held.bone, anchor))
		{
			return;
		}
		const tmath::vector3df target(anchor[0] * grip[0] + anchor[4] * grip[1] + anchor[8] * grip[2] + anchor[12],
			anchor[1] * grip[0] + anchor[5] * grip[1] + anchor[9] * grip[2] + anchor[13],
			anchor[2] * grip[0] + anchor[6] * grip[1] + anchor[10] * grip[2] + anchor[14]);
		const tmath::vector3df end = position_of(hand);
		const tmath::vector3df gap = target - end;
		m_handOff = 100.0f * std::sqrt(gap.x * gap.x + gap.y * gap.y + gap.z * gap.z);
		if (!m_lines) return;
		const uint32_t arm = 0xff40e060, off = m_handOff <= 3.0f ? 0xff40e0ff : 0xff4040ff;   // ABGR: green, yellow, red
		m_graphics->draw_line3d(position_of(upper), position_of(lower), arm, false);
		m_graphics->draw_line3d(position_of(lower), end, arm, false);
		m_graphics->draw_line3d(end, target, off, false);
		// a small cross at the grip
		const float s = 0.03f;
		m_graphics->draw_line3d(target - tmath::vector3df(s, 0.0f, 0.0f), target + tmath::vector3df(s, 0.0f, 0.0f), off, false);
		m_graphics->draw_line3d(target - tmath::vector3df(0.0f, s, 0.0f), target + tmath::vector3df(0.0f, s, 0.0f), off, false);
		m_graphics->draw_line3d(target - tmath::vector3df(0.0f, 0.0f, s), target + tmath::vector3df(0.0f, 0.0f, s), off, false);
	}

	void CharacterViewerState::fill_panels()
	{
		if (!m_ui) return;
		const Entry& entry = m_entries[m_entry];
		const bool heroes = m_mode == Mode::Heroes;
		const unsigned doc = kViewerDocument;
		m_ui->set_text(doc, "v_title", heroes ? "HEROES" : "ZOMBIEPEDIA");
		m_ui->set_text(doc, "v_name", entry.title);
		m_ui->set_text(doc, "v_count", std::to_string(m_entry + 1) + " / " + std::to_string(m_entries.size()));

		// the left panel: a hero's portrait, story and state, or an enemy's numbers and attack
		m_ui->set_class(doc, "v_portrait", "hidden", !heroes);
		bool canChoose = false;
		if (heroes)
		{
			m_ui->set_attribute(doc, "v_portrait", "src", "images/hero_" + entry.id + ".png");
			const Session& session = Session::get();
			const bool rescued = session.profile.heroes.count(entry.id) != 0;
			const bool playing = session.profile.hero == entry.id;
			canChoose = rescued && !playing;
			std::string status;
			if (playing) status = "<div class=\"v_status\">PLAYING</div>";
			else if (rescued) status = "<div class=\"v_status\">Rescued: you can play as " + escape(entry.title) + "</div>";
			else
			{
				const MissionDef* rescue = session.campaign.rescue_of(entry.id);
				status = "<div class=\"v_status locked\">Not rescued yet" + (rescue ? ": " + escape(rescue->title) : std::string()) + "</div>";
			}
			m_ui->set_markup(doc, "v_text", escape(entry.text) + status);
			m_ui->set_markup(doc, "v_stats", "");
			m_ui->set_text(doc, "v_choose", "PLAY AS " + entry.title);
		}
		else
		{
			m_ui->set_markup(doc, "v_text", "");
			auto bar = [](const char* name, const char* kind, float value, float most, const std::string& shown)
			{
				// the square root of the share: the weakest still show (a bear has thirty times the health of a walker)
				const int percent = static_cast<int>(100.0f * std::clamp(std::sqrt(value / std::max(most, 0.001f)), 0.03f, 1.0f));
				return "<div class=\"v_stat\"><span class=\"v_stat_value\">" + shown + "</span><div class=\"v_stat_name\">" + name
					+ "</div><div class=\"v_bar\"><div class=\"v_fill " + kind + "\" style=\"width: " + std::to_string(percent) + "%;\"></div></div></div>";
			};
			std::string stats = bar("HEALTH", "health", entry.health, m_most[0], number(entry.health))
				+ bar("SPEED", "speed", entry.speed, m_most[1], number(entry.speed) + " m/s")
				+ bar("DAMAGE", "damage", entry.damage, m_most[2], number(entry.damage));
			if (!entry.attack.empty()) stats += "<div class=\"v_attack\"><div class=\"title\">ATTACK</div>" + escape(entry.attack) + "</div>";
			m_ui->set_markup(doc, "v_stats", stats);
		}
		m_ui->set_class(doc, "v_choose", "hidden", !canChoose);

		// the right panel: the clip, what it holds (a hero) or its model (an enemy), and the toggles
		const int clip = m_model ? m_model->get_current_animation() : -1;
		m_ui->set_text(doc, "v_clip", clip >= 0 ? m_model->get_animation_name(clip) : std::string("-"));
		char info[96];
		std::snprintf(info, sizeof(info), "%d of %d%s   %.2f s   x%.3g", clip + 1, m_model ? m_model->get_animation_count() : 0,
			m_clip < 0 ? " (rest)" : "", clip >= 0 ? m_model->get_clip_length_seconds(clip) : 0.0, m_speed);
		m_ui->set_text(doc, "v_clip_info", info);
		m_ui->set_text(doc, "v_second_label", heroes ? "HOLDS" : "MODEL");
		if (heroes)
		{
			m_ui->set_text(doc, "v_second", m_weapons[m_weapon].name);
			m_ui->set_text(doc, "v_second_info", std::to_string(m_weapon + 1) + " of " + std::to_string(m_weapons.size()));
		}
		else
		{
			const size_t models = entry.models.size();
			m_ui->set_text(doc, "v_second", std::to_string(m_variant + 1) + " of " + std::to_string(models));
			m_ui->set_text(doc, "v_second_info", file_stem(entry.models[std::min(m_variant, models - 1)]));
		}
		m_ui->set_class(doc, "v_reach_box", "hidden", !heroes);
		m_ui->set_class(doc, "v_reach", "hidden", !heroes);
		m_ui->set_class(doc, "v_lines", "hidden", !heroes);
		m_ui->set_class(doc, "v_pause", "on", m_paused);
		m_ui->set_class(doc, "v_spin", "on", m_spin);
		m_ui->set_class(doc, "v_reach", "on", m_reach);
		m_ui->set_class(doc, "v_lines", "on", m_lines);
		m_reachShown.clear();
		fill_reach();
	}

	// How far the other hand is from the grip (it changes as the clip plays)
	void CharacterViewerState::fill_reach()
	{
		if (!m_ui || m_mode != Mode::Heroes) return;
		char text[96];
		if (m_handOff < 0.0f) std::snprintf(text, sizeof(text), "No grip for the other hand");
		else std::snprintf(text, sizeof(text), "Other hand %s: %.1f cm from the grip", m_reach ? "on" : "off", m_handOff);
		if (text == m_reachShown) return;
		m_reachShown = text;
		m_ui->set_text(kViewerDocument, "v_reach_info", text);
		m_ui->set_class(kViewerDocument, "v_reach_info", "good", m_handOff >= 0.0f && m_handOff <= 3.0f);
		m_ui->set_class(kViewerDocument, "v_reach_info", "bad", m_handOff > 3.0f);
	}

	void CharacterViewerState::on_terminate(Engine* engine)
	{
		(void)engine;
		if (m_keys) m_keys->stop();
		if (m_ui)
		{
			m_ui->remove_click_listener(m_clicks);
			m_ui->unload_document(kViewerDocument);
		}
		if (!m_graphics) return;
		auto task = m_graphics->execute_block([this]()
		{
			m_graphics->clear_active_camera(m_camera.get());
			m_graphics->set_post_process(BgfxGraphicsSystem::PostSettings());
			m_graphics->set_shadows(BgfxGraphicsSystem::ShadowSettings());
			m_weaponMesh.reset();
			m_model.reset();
			m_floor.reset();
			m_camera.reset();
		});
		if (task) task->wait();
	}
}
