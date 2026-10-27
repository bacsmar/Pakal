///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: DungeonLevelState.cpp
// Purpose: Plays the dungeon prototype - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "DungeonLevelState.h"
#include "DungeonMenuState.h"

#include "Engine.h"
#include "GameStateManager.h"
#include "IInputManager.h"
#include "IUIManager.h"
#include "LogMgr.h"
#include "Profiler.h"
#include "bgfx/BgfxGraphicsSystem.h"
#include "bgfx/MeshComponent3D_Bgfx.h"
#include "box2D/PhysicsWorld2D.h"
#include "gameplay/NavGrid2D.h"
#include "net/ByteStream.h"
#include "topdown/view/EffectsSystem.h"
#include "topdown/view/RenderSpace.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>

using namespace Pakal;
using namespace Pakal::TopDown;

namespace Dungeon
{
	namespace
	{
		const char* kRulesFile = "Assets/dungeon/dungeon.json";
		const char* kHudFile = "Assets/dungeon/ui/hud.rml";
		const unsigned kHudDocument = 11;
		const float kPi = 3.14159265f;

		float env_number(const char* name, float fallback)
		{
			const char* value = std::getenv(name);
			return value ? static_cast<float>(std::atof(value)) : fallback;
		}

		std::string percent(float share)
		{
			char text[16];
			std::snprintf(text, sizeof(text), "%.1f%%", 100.0f * std::clamp(share, 0.0f, 1.0f));
			return text;
		}
	}

	DungeonLevelState::DungeonLevelState(uint32_t seed, Session session) : BaseGameState("DungeonLevel"), m_seed(seed), m_session(std::move(session))
	{
	}

	DungeonLevelState::~DungeonLevelState() = default;

	void DungeonLevelState::on_initialize(Engine* engine)
	{
		m_engine = engine;
		m_autoplay = std::getenv("DUNGEON_AUTOPLAY") != nullptr;
		if (m_seed == 0) m_seed = static_cast<uint32_t>(env_number("DUNGEON_SEED", 2026.0f));
		// DUNGEON_CONNECT=host[:port]: a client of that server, without the menu
		const char* address = std::getenv("DUNGEON_CONNECT");
		if (address && m_session.mode == Session::Mode::Alone)
		{
			m_session.mode = Session::Mode::Join;
			m_session.host = address;
			const size_t colon = m_session.host.rfind(':');
			if (colon != std::string::npos)
			{
				m_session.port = static_cast<uint16_t>(std::atoi(m_session.host.c_str() + colon + 1));
				m_session.host = m_session.host.substr(0, colon);
			}
		}
		// the move stick; no aim stick: the abilities are buttons, and aim at the nearest monster (DungeonGame::auto_aim)
		if (IInputManager* input = engine->input_manager())
		{
			input->set_touch_controls(true);
			input->set_touch_aim(false);
		}
	}

	void DungeonLevelState::on_activate(Engine* engine)
	{
		(void)engine;
	}

	void DungeonLevelState::on_deactivate(Engine* engine)
	{
		(void)engine;
	}

	bool DungeonLevelState::load()
	{
		std::ifstream file(kRulesFile);
		if (!file)
		{
			LOG_ERROR("[Dungeon] cannot read %s", kRulesFile);
			return false;
		}
		m_rules = DungeonRules::from_json(nlohmann::json::parse(file, nullptr, false));

		// From above and a little behind, as the genre looks. The mouse aims and strikes; the keys use the abilities.
		const nlohmann::json camera = m_rules.visuals.value("camera", nlohmann::json::object());
		LevelRuntime::Settings settings;
		settings.camera.side = camera.value("side", 0.0f);
		settings.camera.height = camera.value("height", 13.0f);
		settings.camera.back = camera.value("back", 8.5f);
		settings.camera.fovY = camera.value("fovY", 38.0f);
		settings.input.ignoreKeys = m_autoplay;
		settings.input.aimKeys = false;
		settings.input.buttons = { { Key::Num1, 0 }, { Key::Num2, 1 }, { Key::Num3, 2 }, { Key::Num4, 3 }, { Key::Q, 1 }, { Key::E, 2 }, { Key::F, 3 } };
		settings.timeScale = env_number("DUNGEON_TIMESCALE", 1.0f);
		settings.seed = m_seed;
		m_runtime = std::make_unique<LevelRuntime>(*m_engine);
		if (!m_runtime->begin(settings)) return false;

		std::vector<std::string> models;
		for (const auto& [type, creature] : m_rules.creatures) models.push_back(creature.look.value("model", std::string()));
		m_runtime->prefetch(models);

		// the HUD (the fonts are the Zombies example's, as are the characters)
		if (IUIManager* ui = m_engine->get_ui_manager())
		{
			for (const char* font : { "Assets/ui/fonts/Roboto-Regular.ttf", "Assets/ui/fonts/Roboto-Bold.ttf", "Assets/ui/fonts/Bangers-Regular.ttf" }) ui->load_font(font);
			m_hud = std::make_unique<UiPanel>(ui, kHudDocument);
			m_hud->load(kHudFile, "dungeon");
			for (size_t i = 0; i < m_rules.buttons.size() && i < 4; ++i) m_hud->text("d_ab_name" + std::to_string(i), m_rules.buttons[i]);
		}

		// online: the dungeon is built when the server says its seed
		if (m_session.mode != Session::Mode::Alone) return go_online();
		build_world(m_seed, 1, true);
		return true;
	}

	// A client of the session's server; hosting, the server starts here first (on the default port, or any free one)
	bool DungeonLevelState::go_online()
	{
		std::string host = m_session.host;
		uint16_t port = m_session.port;
		if (m_session.mode == Session::Mode::Host)
		{
			m_host = std::make_unique<DungeonHost>();
			if (!m_host->start(m_rules, m_seed, port, 4, device_name()) && !m_host->start(m_rules, m_seed, 0, 4, device_name()))
			{
				LOG_ERROR("[Dungeon] cannot host a game");
				m_lost = true;
				return true;
			}
			host = "127.0.0.1";
			port = m_host->port();
			LOG_INFO("[Dungeon] hosting on port %u", static_cast<unsigned>(port));
		}
		m_client = std::make_unique<Net::NetClient>();
		if (!m_client->connect(host, port, device_name()))
		{
			LOG_ERROR("[Dungeon] cannot reach %s:%u", host.c_str(), static_cast<unsigned>(port));
			m_lost = true;
			return true;
		}
		LOG_INFO("[Dungeon] joining %s:%u", host.c_str(), static_cast<unsigned>(port));
		if (m_hud) m_hud->text("d_monsters", "joining " + host + "...");
		return true;
	}

	void DungeonLevelState::back_to_menu()
	{
		get_manager()->transition_to_state(new DungeonMenuState(), true);
	}

	// An ability's box tapped (its id, or a part's, ends in the button's number) holds that button a moment
	std::vector<std::string> DungeonLevelState::take_ability_taps()
	{
		std::vector<std::string> others;
		if (!m_hud) return others;
		for (const auto& clicked : m_hud->clicks())
		{
			const bool ability = clicked.rfind("d_ab", 0) == 0 || clicked.rfind("d_cd", 0) == 0;
			const char last = clicked.empty() ? ' ' : clicked.back();
			if (ability && last >= '0' && last <= '3') m_runtime->input().tap(last - '0');
			else others.push_back(clicked);
		}
		return others;
	}

	// The dungeon of the seed and its look; with 'heroes' and 'monsters', its characters and their models (a client builds
	// only the dungeon: its characters are the server's)
	void DungeonLevelState::build_world(uint32_t seed, int heroes, bool monsters)
	{
		m_seed = seed;
		m_world = std::make_unique<PhysicsWorld2D>(b2Vec2(0.0f, 0.0f));
		m_nav = std::make_unique<NavGrid2D>(0.5f);
		m_game = std::make_unique<DungeonGame>(*m_world, *m_nav, m_rules, m_seed);
		m_game->start(heroes, monsters);
		m_runtime->on_graphics([this]()
		{
			SceneLook look = SceneLook::from_json(m_rules.visuals.value("lighting", nlohmann::json::object()), m_rules.visuals.value("post", nlohmann::json::object()));
			// The ambient occlusion is baked (bake_occlusion); the screen's (SSAO) only at ultra, where there is power to
			// spare, or with DUNGEON_SSAO=on (off: never)
			const char* ssao = std::getenv("DUNGEON_SSAO");
			const bool screenAo = ssao ? std::string(ssao) == "on" : BgfxGraphicsSystem::default_quality() == BgfxGraphicsSystem::Quality::Ultra;
			if (!screenAo) look.post.aoStrength = 0.0f;
			m_runtime->apply_look(look);
			bake_occlusion(look);
			build_dungeon();
			for (const auto& actor : m_game->actors()) add_character_view(*actor);
			// the bursts of fireballs and the blows all around (no streaks or casings: nobody shoots)
			m_effects = std::make_unique<EffectsSystem>(&m_runtime->graphics(), [this](const std::string& model) { return m_runtime->model(model); },
				std::string(), std::string(), std::string(), RandomStreams::seed_for(m_seed, "fx"));
		});
		m_game->events.subscribe<ExplosionEvent>([this](const ExplosionEvent& blast)
		{
			if (m_effects) m_effects->on_blast(blast.at, blast.radius);
			m_runtime->rig().shake(4.0f, 160.0f);
		});
		m_game->events.subscribe<AreaEvent>([this](const AreaEvent& area)
		{
			if (m_effects) m_effects->on_shockwave(area.at, area.radius);
			m_runtime->rig().shake(3.0f, 140.0f);
		});
		m_game->events.subscribe<ImpactEvent>([this](const ImpactEvent& impact) { if (m_effects) m_effects->on_impact(impact.at, impact.direction, impact.kind); });
		LOG_INFO("[Dungeon] seed %u: %zu rooms, %d monsters", m_seed, m_game->layout().rooms.size(), m_game->monsters_left());
	}

	// Graphics thread: a character's model, from its creature's look (by its name: the replicas of a client have no game
	// of their own to ask)
	void DungeonLevelState::add_character_view(Actor& actor)
	{
		if (actor.kind != ActorKind::Character) return;
		auto creature = m_rules.creatures.find(actor.name);
		if (creature == m_rules.creatures.end()) return;
		const nlohmann::json material = m_rules.visuals.value("characters", nlohmann::json::object());
		const nlohmann::json& look = creature->second.look;
		m_runtime->add_view(actor, look, CharacterViewComponent::Look(), {}, look.value("clay", material.value("clay", 0.0f)));
	}

	void DungeonLevelState::bake_occlusion(const SceneLook& look)
	{
		const char* off = std::getenv("DUNGEON_BAKED_AO");
		if (off && std::string(off) == "off") return;
		const DungeonLayout& layout = m_game->layout();
		const float voxel = 0.5f;                       // meters a side
		const float tall = 3.2f;                        // the walls' height, and the grid's
		const int perCell = std::max(1, static_cast<int>(std::lround(layout.cell / voxel)));
		const int nx = layout.width * perCell, ny = layout.height * perCell, nz = 4;
		if (nx <= 0 || ny <= 0 || nx > 2048 || ny > 2048) return;
		auto open = [&](float x, float y)
		{
			return layout.is_floor(static_cast<int>(std::floor(x / layout.cell)), static_cast<int>(std::floor(y / layout.cell)));
		};
		// the light that comes down to the floor: mostly the sky's, a little of the ground's (stored at half, as the grid is)
		float base[3];
		for (int c = 0; c < 3; ++c) base[c] = 0.75f * look.ambient[c] + 0.25f * look.ambientGround[c];
		const float reach = 2.0f;                       // meters a wall darkens to
		std::vector<uint8_t> bytes(static_cast<size_t>(nx) * ny * nz * 4);
		for (int z = 0; z < nz; ++z)
		{
			// near the floor the walls take more of the sky than at a man's height
			const float height = (static_cast<float>(z) + 0.5f) * tall / static_cast<float>(nz);
			const float lowness = 1.0f - std::min(1.0f, height / tall);
			for (int y = 0; y < ny; ++y)
			{
				for (int x = 0; x < nx; ++x)
				{
					const float px = (static_cast<float>(x) + 0.5f) * voxel, py = (static_cast<float>(y) + 0.5f) * voxel;
					float light = 0.0f;
					if (open(px, py))
					{
						// 8 directions along the floor: how soon a wall closes each one
						float occlusion = 0.0f;
						for (int d = 0; d < 8; ++d)
						{
							const float a = 0.785398f * static_cast<float>(d);
							const float dx = std::cos(a), dy = std::sin(a);
							for (float t = 0.25f; t <= reach; t += 0.25f)
							{
								if (!open(px + dx * t, py + dy * t))
								{
									occlusion += 1.0f - t / reach;
									break;
								}
							}
						}
						light = std::clamp(1.0f - (0.5f + 0.35f * lowness) * occlusion / 4.0f, 0.3f, 1.0f);
					}
					uint8_t* texel = &bytes[(static_cast<size_t>(z) * ny * nx + static_cast<size_t>(y) * nx + x) * 4];
					for (int c = 0; c < 3; ++c) texel[c] = static_cast<uint8_t>(std::clamp(0.5f * base[c] * light * 255.0f + 0.5f, 0.0f, 255.0f));
					texel[3] = 255;
				}
			}
		}
		// level axes (x east, y north, height) to render space (x east, y up, z south)
		const float origin[3] = { 0.0f, 0.0f, 0.0f };
		const float size[3] = { static_cast<float>(nx) * voxel, tall, -static_cast<float>(ny) * voxel };
		m_runtime->graphics().set_gi_volume(bytes.data(), static_cast<uint16_t>(nx), static_cast<uint16_t>(ny), static_cast<uint16_t>(nz), origin, size, 1.0f);
	}

	// Centered on its point of the floor, turned 'yaw' (radians), standing on the floor
	void DungeonLevelState::place(MeshComponent3D_Bgfx& mesh, const b2Vec2& at, float yaw)
	{
		float low[3], high[3];
		mesh.get_bounds(low, high);
		const tmath::vector3df p = floor_point(at);
		const float cx = 0.5f * (low[0] + high[0]), cz = 0.5f * (low[2] + high[2]);
		const float c = std::cos(yaw), s = std::sin(yaw);
		mesh.set_rotation(0.0f, yaw, 0.0f);
		mesh.set_position(p.x - (c * cx + s * cz), p.y - low[1], p.z - (-s * cx + c * cz));
	}

	void DungeonLevelState::build_dungeon()
	{
		const nlohmann::json pieces = m_rules.visuals.value("pieces", nlohmann::json::object());
		const std::string folder = pieces.value("folder", std::string());
		const std::string floorModel = folder + pieces.value("floor", std::string());
		const std::string wallModel = folder + pieces.value("wall", std::string());
		const DungeonLayout& layout = m_game->layout();
		const float half = 0.5f * layout.cell;
		auto add = [&](const std::string& model, const b2Vec2& at, float yaw) -> MeshComponent3D_Bgfx*
		{
			auto mesh = m_runtime->model(model);
			if (!mesh) return nullptr;
			place(*mesh, at, yaw);
			m_pieces.push_back(std::move(mesh));
			return m_pieces.back().get();
		};
		// the floor's tiles and the props are merged by model, a block of 8 x 8 cells at a time (one draw for many, still
		// culled block by block); the walls stay one by one, as each fades when it hides the hero. DUNGEON_MERGE=off keeps
		// every piece on its own.
		static const bool mergePieces = !std::getenv("DUNGEON_MERGE") || std::string(std::getenv("DUNGEON_MERGE")) != "off";
		std::map<std::pair<std::string, int>, std::vector<std::unique_ptr<MeshComponent3D_Bgfx>>> blocks;
		auto addMerged = [&](const std::string& model, const b2Vec2& at, float yaw)
		{
			if (!mergePieces)
			{
				add(model, at, yaw);
				return;
			}
			auto mesh = m_runtime->model(model);
			if (!mesh) return;
			place(*mesh, at, yaw);
			const int block = static_cast<int>(at.x / (8.0f * layout.cell)) * 1000 + static_cast<int>(at.y / (8.0f * layout.cell));
			blocks[{ model, block }].push_back(std::move(mesh));
		};
		// a wall that may hide the hero fades: its box, in render space
		auto fades = [&](MeshComponent3D_Bgfx* mesh, const b2Vec2& at, bool alongX)
		{
			if (!mesh) return;
			OccluderFade::Occluder occluder;
			occluder.mesh = mesh;
			const float w = alongX ? half : 0.25f, d = alongX ? 0.25f : half;
			occluder.low = Vec3{ at.x - w, 0.0f, -at.y - d };
			occluder.high = Vec3{ at.x + w, 3.2f, -at.y + d };
			occluder.faded = 0.12f;
			m_walls.add(occluder);
		};
		for (int y = 0; y < layout.height; ++y)
		{
			for (int x = 0; x < layout.width; ++x)
			{
				if (!layout.is_floor(x, y)) continue;
				const b2Vec2 c = layout.center(x, y);
				addMerged(floorModel, c, 0.0f);
				// a wall on each open side, its face to the floor
				if (!layout.is_floor(x, y + 1)) { const b2Vec2 at = c + b2Vec2(0.0f, half); fades(add(wallModel, at, 0.0f), at, true); }
				if (!layout.is_floor(x, y - 1)) { const b2Vec2 at = c - b2Vec2(0.0f, half); fades(add(wallModel, at, kPi), at, true); }
				if (!layout.is_floor(x + 1, y)) { const b2Vec2 at = c + b2Vec2(half, 0.0f); fades(add(wallModel, at, -0.5f * kPi), at, false); }
				if (!layout.is_floor(x - 1, y)) { const b2Vec2 at = c - b2Vec2(half, 0.0f); fades(add(wallModel, at, 0.5f * kPi), at, false); }
			}
		}
		// a few props in the corners of each room (no bodies: they stand where nobody walks)
		const std::vector<std::string> props = pieces.value("props", std::vector<std::string>());
		const int perRoom = pieces.value("propsPerRoom", 2);
		RandomStream& random = m_runtime->random()["props"];
		for (const auto& room : layout.rooms)
		{
			const b2Vec2 corners[] = { layout.center(room.x, room.y), layout.center(room.x + room.w - 1, room.y),
				layout.center(room.x, room.y + room.h - 1), layout.center(room.x + room.w - 1, room.y + room.h - 1) };
			for (int i = 0; i < perRoom && !props.empty(); ++i)
			{
				const b2Vec2 corner = corners[std::min(3, static_cast<int>(random.uniform() * 4.0f))];
				const b2Vec2 inward = layout.room_center(room) - corner;
				b2Vec2 nudge = inward;
				nudge.Normalize();
				const std::string& prop = props[std::min(props.size() - 1, static_cast<size_t>(random.uniform() * static_cast<float>(props.size())))];
				addMerged(folder + prop, corner - 0.35f * nudge + b2Vec2(random.uniform(-0.2f, 0.2f), random.uniform(-0.2f, 0.2f)), random.uniform(0.0f, 2.0f * kPi));
			}
		}
		size_t merged = 0;
		for (auto& [key, copies] : blocks)
		{
			std::vector<MeshComponent3D_Bgfx*> parts;
			for (const auto& copy : copies) parts.push_back(copy.get());
			auto whole = copies.size() > 1 ? MeshComponent3D_Bgfx::merge(&m_runtime->graphics(), parts) : nullptr;
			if (!whole)
			{
				for (auto& copy : copies) m_pieces.push_back(std::move(copy));   // a lone piece, or a model too big to merge
				continue;
			}
			merged += copies.size();
			m_pieces.push_back(std::move(whole));
			// one copy stays, hidden: it holds the model whose textures the merged one draws with
			copies.front()->set_visible(false);
			m_pieces.push_back(std::move(copies.front()));
		}
		LOG_INFO("[Dungeon] %zu pieces (%zu merged into blocks)", m_pieces.size(), merged);
	}

	void DungeonLevelState::on_update(unsigned long dtMilliseconds)
	{
		(void)dtMilliseconds;
		if (!m_loadTried)
		{
			// loaded on the first update: the models are made on the graphics thread, which is running by now
			m_loadTried = true;
			m_loaded = load();
		}
		if (!m_loaded) return;
		const bool online = m_session.mode != Session::Mode::Alone;

		// the keys: R a new dungeon (online: the menu), I the bag, P or Escape the pause; and the HUD's buttons
		const bool restart = m_runtime->input().key_down(Key::R);
		bool again = restart && !m_restartHeld;
		m_restartHeld = restart;
		const bool inventory = m_runtime->input().key_down(Key::I);
		if (inventory && !m_inventoryHeld) m_inventoryOpen = !m_inventoryOpen;
		m_inventoryHeld = inventory;
		const bool pause = m_runtime->input().key_down(Key::P) || m_runtime->input().key_down(Key::Escape);
		if (pause && !m_pauseHeld) m_paused = !m_paused;
		m_pauseHeld = pause;
		bool leave = false;
		for (const auto& clicked : take_ability_taps())
		{
			if (clicked == "b_again" || clicked == "p_new") again = true;
			else if (clicked == "p_menu") leave = true;
			else if (clicked == "d_pause_button") m_paused = !m_paused;
			else if (clicked == "p_resume") m_paused = false;
			else if (clicked == "d_bag_button") m_inventoryOpen = !m_inventoryOpen;
			else if (clicked == "d_close") m_inventoryOpen = false;
			else if (clicked.rfind("bag_", 0) == 0)
			{
				// a bag item worn, a worn one taken off
				const size_t index = static_cast<size_t>(std::atoi(clicked.c_str() + 4));
				if (index < m_own.bag.size()) send_gear(DungeonGame::GearCommand::Wear, m_own.bag[index]);
			}
			else if (clicked.rfind("worn_", 0) == 0) send_gear(DungeonGame::GearCommand::TakeOff, clicked.substr(5));
		}
		if (leave || (online && again))
		{
			back_to_menu();
			return;   // this state is gone
		}
		if (again)
		{
			// a new dungeon (the next seed), or the same one again after a defeat
			const uint32_t next = m_game && m_game->won() ? m_seed + 1 : m_seed;
			get_manager()->transition_to_state(new DungeonLevelState(next), true);
			return;   // this state is gone
		}
		if (m_hud)
		{
			m_hud->show("d_pause", m_paused);
			m_hud->show("p_new", !online);
			m_hud->text("p_note", online ? "the others play on: the dungeon does not wait" : "");
		}

		if (online)
		{
			if (m_client && !m_lost) update_online();
			if (m_lost)
			{
				// the server went away, or never answered: the result panel says so
				if (m_client) m_client->close();
				if (m_hud)
				{
					m_hud->show("d_result", true);
					m_hud->text("d_result_title", m_game ? "DISCONNECTED" : "NO SERVER");
					m_hud->text("b_again", "MENU");
				}
			}
			return;
		}
		if (m_paused)
		{
			m_runtime->advance();   // the time paused does not pile up for later
			return;
		}

		for (int steps = m_runtime->advance(); steps > 0; --steps)
		{
			PAKAL_PROFILE_SCOPE("sim.step");
			PlayerInput input = m_autoplay ? m_bot.think(*m_game, m_runtime->step_seconds()) : m_runtime->read_input();
			m_game->step(m_runtime->step_seconds(), input);
		}
		const Actor* hero = m_game->player();
		draw_frame(*m_game, hero);
		OwnState own;
		if (hero)
		{
			ByteWriter out;
			write_own(out, *hero, m_rules);
			read_own(out.data(), own);
		}
		update_hud(own, m_game->won(), m_game->lost(), m_game->monsters_left());
		// what just happened (an item, a level, a chest), for a few seconds
		if (m_game->messages != m_shownMessages)
		{
			m_shownMessages = m_game->messages;
			show_message(m_game->message);
		}
		update_inventory(own);

		if (!m_reported && (m_game->won() || m_game->lost()))
		{
			m_reported = true;
			const ProgressionComponent* growth = hero ? hero->get<ProgressionComponent>() : nullptr;
			LOG_INFO("[Dungeon] %s after %.1f s: %d kills, level %d, %d items, %d chests", m_game->won() ? "DUNGEON CLEARED" : "YOU DIED", m_game->timeMs / 1000.0,
				m_game->kills, growth ? growth->level() : 0, m_game->itemsTaken, m_game->chestsOpened);
		}
	}

	void DungeonLevelState::send_gear(DungeonGame::GearCommand command, const std::string& what)
	{
		const std::vector<uint8_t> bytes = DungeonGame::gear_command(command, what);
		if (m_client)
		{
			m_client->send_command(bytes);
			return;
		}
		if (Actor* hero = m_game ? m_game->player() : nullptr)
		{
			ByteReader in(bytes);
			DungeonGame::apply_gear_command(*hero, in);
		}
	}

	// A client: the server's world as its snapshots draw it. The dungeon is built from the seed the server says; its
	// characters are replicas (their models made as they come, let go when they leave); this player's input goes each step.
	void DungeonLevelState::update_online()
	{
		const int steps = m_runtime->advance();
		{
			PAKAL_PROFILE_SCOPE("net.poll");
			m_client->poll(m_runtime->frame_seconds());
		}
		m_onlineSeconds += m_runtime->frame_seconds();
		if (m_client->welcomed() ? !m_client->connected() : m_onlineSeconds > 8.0)
		{
			m_lost = true;
			return;
		}
		if (!m_game && m_client->welcomed())
		{
			uint32_t seed = m_seed;
			read_game_info(m_client->welcome().game, seed);
			build_world(seed, 0, false);
			m_replica = std::make_unique<Net::ReplicaGame>();
		}
		if (!m_game)
		{
			if (m_hud && !m_client->refused().empty()) m_hud->text("d_monsters", "refused: " + m_client->refused());
			return;
		}
		for (int i = 0; i < steps; ++i) m_client->send_input(m_runtime->read_input());

		std::vector<Actor*> fresh;
		m_replica->apply(m_client->actors(), [&](Actor& actor) { fresh.push_back(&actor); });
		if (!fresh.empty() || !m_replica->missing().empty())
		{
			m_runtime->on_graphics([&]()
			{
				for (Actor* actor : fresh) add_character_view(*actor);
				m_replica->forget_missing();
			});
		}
		const Actor* hero = m_replica->replica(m_client->welcome().actor);
		draw_frame(*m_replica, hero);
		OwnState own;
		read_own(m_client->own(), own);
		// over: the boss is down, or this hero is
		bool won = false;
		int left = 0;
		for (const auto& actor : m_replica->actors())
		{
			const Net::ActorState* state = m_replica->state(*actor);
			if (!state || actor->kind != ActorKind::Character || state->faction == m_game->heroes) continue;
			if (!state->dead) ++left;
			if (state->dead && actor->name == m_rules.boss) won = true;
		}
		update_hud(own, won, !won && hero && own.health <= 0.0f, left);
		if (own.level != m_shownLevel && m_shownLevel > 0) show_message("LEVEL " + std::to_string(own.level));
		m_shownLevel = own.level;
		// the bag is the server's: the snapshots show it, a click on it is a command to the server
		update_inventory(own);
		if (m_hud) m_hud->text("b_again", "MENU");
	}

	// What both modes draw each frame: the characters, the loot, the missiles, the effects, the camera on the hero and the
	// walls that hide it; and a capture when asked
	void DungeonLevelState::draw_frame(TopDownGame& world, const Actor* hero)
	{
		PAKAL_PROFILE_SCOPE("dungeon.frame");   // the views, loot, missiles, blood, chests, effects, camera and walls
		const float dt = m_runtime->frame_seconds();
		m_seconds += dt;
		m_runtime->sync_views(world);
		sync_loot(world);
		draw_missiles(world);
		react(world, hero);
		draw_pools(world, dt);
		sync_chests(world, dt);
		if (m_effects) m_effects->update(dt, {});
		const b2Vec2 at = hero ? hero->drawn_position(m_runtime->alpha()) : b2Vec2(0.0f, 0.0f);
		static const float scale = env_number("DUNGEON_CAM_SCALE", 1.0f);   // debug: below 1 comes closer
		m_runtime->follow(at, m_seconds * 1000.0, 0.9f, scale);
		m_walls.update(m_runtime->rig().eye(), Vec3{ at.x, 1.0f, -at.y }, hero != nullptr, dt);
		// DUNGEON_SCREENSHOT=<file>.tga at DUNGEON_SCREENSHOT_AT seconds
		static const char* shot = std::getenv("DUNGEON_SCREENSHOT");
		if (shot && !m_shot && m_seconds >= env_number("DUNGEON_SCREENSHOT_AT", 3.0f))
		{
			m_shot = true;
			const std::string path = shot;
			m_runtime->on_graphics([this, path]() { m_runtime->graphics().request_screenshot(path.c_str()); });
		}
	}

	// A model for each pile on the floor, gone with it
	void DungeonLevelState::sync_loot(TopDownGame& world)
	{
		std::set<uintptr_t> lying;
		std::vector<const Actor*> fresh;
		for (const auto& actor : world.actors())
		{
			if (actor->kind != ActorKind::Pickup || actor->removed) continue;
			lying.insert(actor->id);
			if (m_loot.find(actor->id) == m_loot.end()) fresh.push_back(actor.get());
		}
		std::vector<uintptr_t> gone;
		for (const auto& [id, mesh] : m_loot)
		{
			if (lying.find(id) == lying.end()) gone.push_back(id);
		}
		if (fresh.empty() && gone.empty()) return;
		const nlohmann::json pieces = m_rules.visuals.value("pieces", nlohmann::json::object());
		m_runtime->on_graphics([&]()
		{
			for (uintptr_t id : gone) m_loot.erase(id);
			for (const Actor* pile : fresh)
			{
				const bool gold = pile->name == "gold";
				auto mesh = m_runtime->model(pieces.value(gold ? "gold" : "item", std::string()));
				if (mesh)
				{
					mesh->set_scale(gold ? 0.35f : 0.45f);
					place(*mesh, pile->position, static_cast<float>(pile->id % 7));
				}
				m_loot[pile->id] = std::move(mesh);
			}
		});
	}

	void DungeonLevelState::react(TopDownGame& world, const Actor* hero)
	{
		if (!m_effects) return;
		auto ability = [&](const std::string& creature, const std::string& name) -> const AbilityRules*
		{
			auto rules = m_rules.creatures.find(creature);
			if (rules == m_rules.creatures.end()) return nullptr;
			for (const auto& one : rules->second.abilities)
			{
				if (one.name == name) return &one;
			}
			return nullptr;
		};
		std::map<uintptr_t, Flying> flying;
		for (const auto& actor : world.actors())
		{
			if (actor->removed) continue;
			if (actor->kind == ActorKind::Missile)
			{
				flying[actor->id] = Flying{ actor->position, actor->name };
				continue;
			}
			if (actor->kind != ActorKind::Character) continue;
			const CharacterViewSource* source = actor->get<CharacterViewSource>();
			if (!source) continue;
			const CharacterViewState state = source->view_state();
			// hurt: blood splashes away from the nearest foe, and the camera jolts when it is this player's hero
			auto seen = m_hurtSeen.find(actor->id);
			if (seen != m_hurtSeen.end() && seen->second != state.hurtClips)
			{
				b2Vec2 away(0.0f, 1.0f);
				float nearest = 4.0f;
				for (const auto& other : world.actors())
				{
					if (other->removed || other->kind != ActorKind::Character || world.faction_of(*other) == world.faction_of(*actor)) continue;
					const b2Vec2 to = actor->position - other->position;
					if (to.Length() < nearest)
					{
						nearest = to.Length();
						away = to;
					}
				}
				away.Normalize();
				m_effects->on_impact(actor->position - 0.2f * away, away, ImpactKind::Enemy);
				if (actor.get() == hero) m_runtime->rig().shake(2.5f, 110.0f);
			}
			m_hurtSeen[actor->id] = state.hurtClips;
			// online: an ability around the user starts its shockwave (alone, the game's AreaEvent does)
			const Net::ActorState* remote = m_replica ? m_replica->state(*actor) : nullptr;
			if (!remote) continue;
			auto acted = m_actionSeen.find(actor->id);
			if (acted != m_actionSeen.end() && acted->second != remote->actionSerial && !remote->action.empty())
			{
				const AbilityRules* used = ability(actor->name, remote->action);
				if (used && used->kind == AbilityKind::Area)
				{
					m_effects->on_shockwave(actor->position, used->radius);
					m_runtime->rig().shake(3.0f, 140.0f);
				}
			}
			m_actionSeen[actor->id] = remote->actionSerial;
		}
		// online: a missile gone from the air burst where it was last
		if (m_replica)
		{
			for (const auto& [id, missile] : m_flying)
			{
				if (flying.count(id)) continue;
				const AbilityRules* thrown = nullptr;
				for (const auto& [type, creature] : m_rules.creatures)
				{
					if (!thrown) thrown = ability(type, missile.name);
				}
				if (thrown && thrown->radius > 0.0f)
				{
					m_effects->on_blast(missile.at, thrown->radius);
					m_runtime->rig().shake(4.0f, 160.0f);
				}
				else m_effects->on_impact(missile.at, b2Vec2(0.0f, 1.0f), ImpactKind::Wall);
			}
		}
		m_flying = std::move(flying);
	}

	void DungeonLevelState::draw_pools(TopDownGame& world, float dt)
	{
		const std::vector<std::string> blood = m_rules.visuals.value("blood", std::vector<std::string>());
		std::vector<std::pair<uintptr_t, b2Vec2>> fresh;
		for (const auto& actor : world.actors())
		{
			if (actor->removed || actor->kind != ActorKind::Character || m_pools.count(actor->id)) continue;
			const CharacterViewSource* source = actor->get<CharacterViewSource>();
			if (!source || !source->view_state().dead) continue;
			Pool& pool = m_pools[actor->id];
			pool.at = actor->position;
			pool.size = 1.2f * std::max(1.0f, world.hit_radius(*actor) / 0.35f);   // meters across: a boss leaves more
			fresh.emplace_back(actor->id, actor->position);
		}
		if (!fresh.empty() && !blood.empty())
		{
			// one of the pack's blood decals, turned at random
			m_runtime->on_graphics([&]()
			{
				for (const auto& [id, at] : fresh)
				{
					Pool& pool = m_pools[id];
					const uint32_t hash = static_cast<uint32_t>(id * 2654435761u);
					pool.mesh = m_runtime->model(blood[hash % blood.size()]);
					if (!pool.mesh) continue;
					pool.mesh->set_rotation(0.0f, static_cast<float>(hash % 628) * 0.01f, 0.0f);
					// from meters across to the model's scale (the decals are of different sizes)
					float low[3], high[3];
					pool.mesh->get_bounds(low, high);
					pool.size /= std::max(0.1f, std::max(high[0] - low[0], high[2] - low[2]));
					const tmath::vector3df p = floor_point(at, 0.03f);   // over the floor tiles (2 cm thick)
					pool.mesh->set_position(p.x, p.y, p.z);
					pool.mesh->set_scale(0.01f);
				}
			});
		}
		for (auto& [id, pool] : m_pools)
		{
			// it spreads for a second and a half after the fall, then stays
			pool.age += dt;
			const float grow = std::min(1.0f, pool.age / 1.5f);
			if (pool.mesh) pool.mesh->set_scale(std::max(0.01f, pool.size * grow * (2.0f - grow)));
		}
	}

	void DungeonLevelState::sync_chests(TopDownGame& world, float dt)
	{
		const nlohmann::json pieces = m_rules.visuals.value("pieces", nlohmann::json::object());
		std::vector<const Actor*> fresh;
		for (const auto& actor : world.actors())
		{
			if (actor->removed || actor->kind != ActorKind::Prop || actor->name != "chest") continue;
			auto found = m_chests.find(actor->id);
			if (found == m_chests.end())
			{
				fresh.push_back(actor.get());
				continue;
			}
			const CharacterViewSource* source = actor->get<CharacterViewSource>();
			const bool opened = source && source->view_state().dead;
			Chest& chest = found->second;
			if (opened && !chest.opened)
			{
				chest.opened = true;
				chest.glow = 1.0f;
				if (chest.mesh) chest.mesh->set_color(0.45f, 0.38f, 0.32f);   // emptied: dark
				if (m_effects) m_effects->on_shockwave(actor->position, 1.2f);
			}
			// a burst of golden sparks that rises and fades
			if (chest.glow > 0.0f)
			{
				chest.glow = std::max(0.0f, chest.glow - dt);
				const float rise = 1.0f - chest.glow;
				for (int i = 0; i < 8; ++i)
				{
					const float a = 0.785f * static_cast<float>(i) + 2.0f * rise;
					const b2Vec2 at = actor->position + (0.25f + 0.35f * rise) * b2Vec2(std::cos(a), std::sin(a));
					const uint8_t alpha = static_cast<uint8_t>(220.0f * chest.glow);
					m_runtime->graphics().draw_particle(floor_point(at, 0.4f + 1.2f * rise), 0.09f, (static_cast<uint32_t>(alpha) << 24) | 0x0040d0ffu, true);
				}
			}
		}
		if (fresh.empty()) return;
		const std::string model = pieces.value("chest", std::string());
		m_runtime->on_graphics([&]()
		{
			for (const Actor* actor : fresh)
			{
				Chest& chest = m_chests[actor->id];
				chest.mesh = m_runtime->model(model);
				if (!chest.mesh) continue;
				// it faces the middle of its room: its back to the wall
				float yaw = 0.0f;
				const DungeonLayout& layout = m_game->layout();
				for (const auto& room : layout.rooms)
				{
					const b2Vec2 low = layout.center(room.x, room.y), high = layout.center(room.x + room.w - 1, room.y + room.h - 1);
					if (actor->position.x < low.x - 0.1f || actor->position.x > high.x + 0.1f || actor->position.y < low.y - 0.1f || actor->position.y > high.y + 0.1f) continue;
					const b2Vec2 in = layout.room_center(room) - actor->position;
					yaw = std::atan2(in.x, in.y);
				}
				chest.mesh->set_scale(1.4f);   // the pack's chest is small for a dungeon's
				place(*chest.mesh, actor->position, yaw);
			}
		});
	}

	// Fireballs and spit: a glowing ball and its fading trail
	void DungeonLevelState::draw_missiles(TopDownGame& world)
	{
		BgfxGraphicsSystem& graphics = m_runtime->graphics();
		for (const auto& actor : world.actors())
		{
			if (actor->kind != ActorKind::Missile || actor->removed) continue;
			const bool fire = actor->name == "fireball";
			const uint32_t core = fire ? 0xff40c0ffu : 0xff60ff90u;   // ABGR
			const uint32_t glow = fire ? 0x802080ffu : 0x8030c060u;
			const b2Vec2 at = actor->drawn_position(m_runtime->alpha());
			b2Vec2 back = actor->previousPosition - actor->position;
			back.Normalize();
			graphics.draw_particle(floor_point(at, 1.1f), fire ? 0.3f : 0.18f, core, true);
			for (int i = 1; i <= 3; ++i)
			{
				graphics.draw_particle(floor_point(at + (0.25f * static_cast<float>(i)) * back, 1.1f), (fire ? 0.28f : 0.15f) / static_cast<float>(i), glow, true);
			}
		}
	}

	void DungeonLevelState::show_message(const std::string& text)
	{
		m_messageSeconds = 2.5f;
		if (m_hud) m_hud->text("d_message", text);
	}

	// The flasks, experience, level, gold, the abilities (their cooldown as a shade, darker while they cannot be paid), the
	// message of the moment and the result
	void DungeonLevelState::update_hud(const OwnState& own, bool won, bool lost, int monstersLeft)
	{
		if (!m_hud) return;
		char text[96];
		m_hud->property("d_health_fill", "height", percent(own.health / std::max(1.0f, own.healthMax)));
		std::snprintf(text, sizeof(text), "%.0f / %.0f", own.health, own.healthMax);
		m_hud->text("d_health_text", text);
		m_hud->property("d_mana_fill", "height", percent(own.mana / std::max(1.0f, own.manaMax)));
		std::snprintf(text, sizeof(text), "%.0f / %.0f", own.mana, own.manaMax);
		m_hud->text("d_mana_text", text);
		m_hud->bar("d_xp_fill", static_cast<float>(own.experience) / static_cast<float>(std::max(1, own.experienceNext)));
		m_hud->text("d_level", "LEVEL " + std::to_string(own.level));
		m_hud->text("d_gold", std::to_string(own.gold) + " GOLD");
		m_hud->text("d_monsters", std::to_string(monstersLeft) + (won ? "" : " monsters  -  the Butcher waits below"));
		for (size_t i = 0; i < own.cooldowns.size() && i < 4; ++i)
		{
			m_hud->property("d_cd" + std::to_string(i), "height", percent(own.cooldowns[i]));
			m_hud->set_class("d_ab" + std::to_string(i), "unpaid", (own.unpaid & (1u << i)) != 0);
		}
		m_messageSeconds = std::max(0.0f, m_messageSeconds - m_runtime->frame_seconds());
		m_hud->show("d_message", m_messageSeconds > 0.0f);
		const bool over = won || lost;
		m_hud->show("d_result", over);
		if (over)
		{
			m_hud->text("d_result_title", won ? "DUNGEON CLEARED" : "YOU DIED");
			m_hud->text("b_again", won ? "NEXT DUNGEON" : "TRY AGAIN");
		}
	}

	// The bag and what is worn, rebuilt only when it changes; an item's bonuses under its name
	void DungeonLevelState::update_inventory(const OwnState& own)
	{
		m_own = own;
		if (!m_hud) return;
		m_hud->show("d_inventory", m_inventoryOpen);
		if (!m_inventoryOpen) return;
		auto describe = [&](const std::string& id, const std::string& element)
		{
			auto found = m_rules.items.find(id);
			const ItemRules* item = found != m_rules.items.end() ? &found->second : nullptr;
			if (!item) return std::string();
			std::string bonuses;
			for (const auto& bonus : item->bonuses)
			{
				char part[64];
				if (bonus.multiply != 1.0f) std::snprintf(part, sizeof(part), "%s x%.2f ", bonus.stat.c_str(), bonus.multiply);
				else std::snprintf(part, sizeof(part), "%s +%.2f ", bonus.stat.c_str(), bonus.add);
				bonuses += part;
			}
			return "<div id=\"" + element + "\" class=\"d_item " + item->rarity + "\"><div class=\"d_item_name\">" + item->name + "</div><div class=\"d_item_slot\">"
				+ item->slot + "</div><div class=\"d_item_bonus\">" + bonuses + "</div></div>";
		};
		std::string worn, bag;
		for (const auto& [slot, id] : own.worn) worn += describe(id, "worn_" + slot);
		for (size_t i = 0; i < own.bag.size(); ++i) bag += describe(own.bag[i], "bag_" + std::to_string(i));
		if (worn + "|" + bag == m_shownInventory) return;
		m_shownInventory = worn + "|" + bag;
		m_hud->markup("d_worn", worn.empty() ? "<div class=\"d_empty\">nothing</div>" : worn);
		m_hud->markup("d_bag", bag.empty() ? "<div class=\"d_empty\">empty</div>" : bag);
	}

	void DungeonLevelState::on_terminate(Engine* engine)
	{
		if (IInputManager* input = engine->input_manager()) input->set_touch_aim(true);
		m_hud.reset();
		// the connection goes before the server it talks to
		m_client.reset();
		m_host.reset();
		// the characters' models go before the prototypes they share; the pieces, the loot and the effects too
		if (m_runtime)
		{
			m_runtime->end([this]()
			{
				m_game.reset();
				m_replica.reset();
				m_walls.clear();
				m_pieces.clear();
				m_pools.clear();
				m_chests.clear();
				m_runtime->graphics().clear_gi_volume();
				m_loot.clear();
				if (m_effects) m_effects->release();
				m_effects.reset();
			});
		}
	}
}
