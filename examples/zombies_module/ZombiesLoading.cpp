///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ZombiesLoading.cpp
// Purpose: The loading of a Zombies level, in steps across frames under the loading screen: the
//          document and the rules, the game, the level's model files a few per frame, the objects and
//          their props, the physics (TopDown's build_level_physics), the player and its looks, the
//          zombie pools, the scripts, the light, the post-process and the camera. Members of the level
//          in a file of their own; the generic loading (an asset cache and a LevelRuntime) is phase 4b.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ZombiesLevelState.h"
#include "IInputManager.h"
#include "LevelLook.h"
#include "Components.h"
#include "topdown/kits/shooter/WeaponComponent.h"
#include "Meta.h"
#include "MenuState.h"
#include "topdown/LevelPhysics.h"
#include "topdown/SpawnSystem.h"
#include "topdown/ActorOrientation.h"
#include "topdown/view/AudioCues.h"
#include "topdown/view/EffectsSystem.h"
#include "topdown/view/PropMesh.h"
#include "topdown/view/RenderSpace.h"

#include "Engine.h"
#include "IUIManager.h"
#include "LogMgr.h"
#include "Profiler.h"
#include "bgfx/BgfxGraphicsSystem.h"
#include "bgfx/CameraComponent3D_Bgfx.h"
#include "bgfx/MeshComponent3D_Bgfx.h"
#include "box2D/PhysicsWorld2D.h"
#include "gameplay/NavGrid2D.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <thread>

using namespace Pakal;

namespace Zombies
{
	namespace
	{
		const float kHalfPi = 1.5707963f;

		// Level units (centimeters, Y forward, Z up) to render space
		tmath::vector3df level_to_render(double x, double y, double z)
		{
			return tmath::vector3df(static_cast<float>(x / kUnitsPerMeter), static_cast<float>(z / kUnitsPerMeter),
				static_cast<float>(-y / kUnitsPerMeter));
		}

		// Column-major 4x4 matrices (bgfx layout), built here so the module does not link bx
		using Mat4 = std::array<float, 16>;

		Mat4 mul(const Mat4& a, const Mat4& b)
		{
			Mat4 out{};
			for (int c = 0; c < 4; ++c)
				for (int r = 0; r < 4; ++r)
				{
					float sum = 0.0f;
					for (int k = 0; k < 4; ++k) sum += a[k * 4 + r] * b[c * 4 + k];
					out[c * 4 + r] = sum;
				}
			return out;
		}

		Mat4 translation(float x, float y, float z) { return Mat4{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1 }; }
		Mat4 scaling(float s) { return Mat4{ s, 0, 0, 0, 0, s, 0, 0, 0, 0, s, 0, 0, 0, 0, 1 }; }

		Mat4 rotation_x(float t)
		{
			const float c = std::cos(t), s = std::sin(t);
			return Mat4{ 1, 0, 0, 0, 0, c, s, 0, 0, -s, c, 0, 0, 0, 0, 1 };
		}

		Mat4 rotation_y(float t)
		{
			const float c = std::cos(t), s = std::sin(t);
			return Mat4{ c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, 0, 0, 0, 1 };
		}

		Mat4 rotation_z(float t)
		{
			const float c = std::cos(t), s = std::sin(t);
			return Mat4{ c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		}

		// Object placement from the level: position (level units), Euler rotation in degrees about the
		// level's X, Y, Z axes (Z up), and uniform scale. Output: column-major matrix in render space.
		// Models are already in meters (Assimp applies the Collada unit), so only the scale is applied.
		Mat4 placement_matrix(const LevelObject& object)
		{
			const float degToRad = 3.14159265f / 180.0f;
			const float rx = static_cast<float>(object.rot[0]) * degToRad;
			const float ry = static_cast<float>(object.rot[1]) * degToRad;
			const float rz = static_cast<float>(object.rot[2]) * degToRad;
			const float s = static_cast<float>(object.scale);
			const tmath::vector3df at = level_to_render(object.x, object.y, object.z);

			// Static models are already Y up (Assimp applies the Collada up axis). The game's rotation is
			// defined in its own Z-up frame, so: translate * toRender * Rz * Ry * Rx * toLevel * scale
			const Mat4 translate = translation(at.x, at.y, at.z);
			const Mat4 toRender = rotation_x(-kHalfPi); // level Z up -> render Y up
			const Mat4 toLevel = rotation_x(kHalfPi);   // model Y up -> level Z up
			return mul(mul(mul(mul(translate, toRender), rotation_z(rz)), rotation_y(ry)), mul(rotation_x(rx), mul(toLevel, scaling(s))));
		}
	}

	void ZombiesLevelState::continue_loading()
	{
		if (m_loadFailed || !m_graphics) return;
		bool ok = true, done = false;
		auto run = [this](std::function<void()> work)
		{
			auto task = m_graphics->execute_block(std::move(work));
			if (task) task->wait();
		};
		switch (m_loadStage)
		{
		case 0:
			run([&]() { ok = load_begin(); });
			if (ok) m_loadStage = 1;
			break;
		case 1:
			run([&]() { done = load_models(40.0); });
			if (done) m_loadStage = 2;
			break;
		default:
			run([&]() { ok = load_finish(); });
			if (ok) loading_done();
			break;
		}
		if (!ok)
		{
			m_loadFailed = true;
			LOG_ERROR("[ZombiesLevel] Level '%s' did not load", m_levelPath.c_str());
			return;
		}
		if (!m_loaded)
		{
			// the document is 5%, the model files up to 90%, the rest after them
			const float models = m_modelQueue.empty() ? 1.0f : static_cast<float>(m_modelsLoaded) / static_cast<float>(m_modelQueue.size());
			show_loading_progress(m_loadStage == 0 ? 0.0f : 0.05f + 0.85f * models);
			// Debug: ZOMBIES_SCREENSHOT_LOADING=<file.tga> captures the loading screen halfway through the model files
			static const char* shot = std::getenv("ZOMBIES_SCREENSHOT_LOADING");
			if (shot && *shot && !m_screenshotTaken && m_loadStage == 1 && models >= 0.5f)
			{
				m_screenshotTaken = true;
				const std::string path = shot;
				m_graphics->execute_block([this, path]() { m_graphics->request_screenshot(path.c_str()); });
			}
		}
	}

	void ZombiesLevelState::show_loading_progress(float progress)
	{
		if (!m_ui) return;
		char width[16];
		std::snprintf(width, sizeof(width), "%.0f%%", 100.0f * std::clamp(progress, 0.0f, 1.0f));
		m_ui->set_property(kLoadingDocument, "loading_fill", "width", width);
	}

	void ZombiesLevelState::loading_done()
	{
		if (m_input) m_input->set_touch_controls(true);   // the sticks while it is played
		m_loaded = true;
		const double loadMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - m_loadStart).count();
		LOG_INFO("[Load] level ready in %.0f ms", loadMs);
		Pakal::Profiler::get().info("level", m_levelPath);
		Pakal::Profiler::get().counter("load.level_ms", loadMs);
		if (m_ui) m_ui->unload_document(kLoadingDocument);
		// a click on the development panel or on a button of the interface does not fire
		m_controls->listen(m_input, m_engine ? m_engine->os_manager() : nullptr, [this](int x, int y)
		{
			return (m_graphics && m_graphics->get_debug_ui().wants_mouse()) || (m_ui && m_ui->is_over_ui(x, y));
		});
		m_hud.load(m_ui);
		m_hud.refresh_boosts(m_missionId);
		m_devPanelId = m_graphics->get_debug_ui().add_panel([this]() { dev_panel(); });
	}

	// The prototype of a model file shares its GPU buffers with every instance; it is not drawn itself (an animated one also
	// shares its skeleton and clips, each instance keeping its own pose). One not asked for at the start of the loading is
	// waited for here (this runs on the graphics thread, which uploads it).
	std::unique_ptr<MeshComponent3D_Bgfx> ZombiesLevelState::create_instance(const std::string& file)
	{
		AssetHandle<MeshComponent3D_Bgfx>& prototype = m_prototypes[file];
		if (!prototype) prototype = m_assetCache->load<MeshComponent3D_Bgfx>(file);
		while (prototype->state() == AssetState::Loading)
		{
			m_assetCache->pump(20.0);
			if (prototype->state() == AssetState::Loading) std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		auto instance = std::make_unique<MeshComponent3D_Bgfx>(m_graphics);
		if (const MeshComponent3D_Bgfx* model = prototype->get())
		{
			if (model->is_loaded()) instance->share_from(*model);
		}
		else
		{
			LOG_WARNING("[ZombiesLevel] model not loaded: %s", file.c_str());
		}
		return instance;
	}

	bool ZombiesLevelState::load_begin()
	{
		// How long each part takes (the log's "[Load]" lines)
		auto last = std::chrono::steady_clock::now();
		auto mark = [&last](const char* part)
		{
			const auto now = std::chrono::steady_clock::now();
			LOG_INFO("[Load] %s %.0f ms (%s)", part, std::chrono::duration<double, std::milli>(now - last).count(),
				MeshComponent3D_Bgfx::take_load_stats().c_str());
			last = now;
		};
		// The level data (any version of the file, upgraded on read) lives in a LevelDocument; a layer over a base comes
		// composed with it (LevelLayers)
		LevelDocument doc;
		std::string loadError;
		if (!load_level(m_levelPath, doc, loadError))
		{
			LOG_ERROR("[ZombiesLevel] %s", loadError.c_str());
			return false;
		}
		for (const std::string& line : doc.validate())
		{
			if (line.rfind("error:", 0) == 0)
			{
				LOG_ERROR("[ZombiesLevel] %s", line.c_str());
			}
		}
		if (!doc.hasSpawn)
		{
			LOG_ERROR("[ZombiesLevel] level has no spawn point");
			return false;
		}
		m_levelName = doc.name;

		// The original game's rules (written by the converter from the object library)
		m_rules.read(doc.extra.value("rules", Json::object()), doc.extra.value("destructibles", Json::array()));
		for (const auto& warning : m_rules.warnings)
		{
			LOG_WARNING("[ZombiesLevel] rules: %s", warning.c_str());
		}
		// A mission plays with what the player bought: the equipped gun with its upgrades, the extra health
		if (!m_missionId.empty())
		{
			const Session& session = Session::get();
			apply_loadout(m_rules, session.campaign, session.profile);
			LOG_INFO("[ZombiesLevel] mission %s: %s (level %d), %.0f health", m_missionId.c_str(), m_rules.gun.name.c_str(),
				session.profile.weapon_level(session.profile.equipped), m_rules.player.health);
		}
		LOG_INFO("[ZombiesLevel] rules: %s %.2f damage every %.0f ms, %zu enemy types, %zu gift entries, %zu props that break",
			m_rules.gun.name.c_str(), m_rules.gun.bulletDamage, m_rules.gun.fireRateMs, m_rules.enemies.size(),
			m_rules.giftTable.size(), m_rules.props.size());

		// The game: the world and the walkable grid are filled in below
		m_world = std::make_unique<PhysicsWorld2D>(b2Vec2(0.0f, 0.0f));
		m_nav = std::make_unique<NavGrid2D>(0.5f);
		m_game = std::make_unique<ZombiesGame>(*m_world, *m_nav, m_rules, m_random.seed());
		// What the game reports (delivered at the end of each step): sounds, and the camera's shakes; the effects listen
		// below, once they are made
		m_game->events.subscribe<SoundEvent>([this](const SoundEvent& sound) { play_sound(sound.name, sound.at); });
		m_game->events.subscribe<ExplosionEvent>([this](const ExplosionEvent& blast)
		{
			m_rig->shake_from(Vec3{ blast.at.x, 0.0f, -blast.at.y }, blast.shake);
			if (m_effects) m_effects->on_blast(blast.at, blast.radius);
		});
		m_game->events.subscribe<ShockwaveEvent>([this](const ShockwaveEvent& wave)
		{
			m_rig->shake_from(Vec3{ wave.at.x, 0.0f, -wave.at.y }, wave.shake);
			if (m_effects) m_effects->on_shockwave(wave.at, wave.radius);
		});
		m_game->events.subscribe<ShakeEvent>([this](const ShakeEvent& shake) { m_rig->shake_from(Vec3{ shake.at.x, 0.0f, -shake.at.y }, shake.power); });
		m_game->spawnEnemy = [this](const std::string& type, const b2Vec2& at) { return spawn_enemy(type, at); };
		m_game->onRemove = [this](Actor& actor)
		{
			// A zombie's model goes back to the pool for the next one
			if (CharacterViewComponent* view = actor.get<CharacterViewComponent>())
			{
				if (auto mesh = view->release_mesh())
				{
					mesh->set_scale(0.0f);
					mesh->set_color(1.0f, 1.0f, 1.0f);
					m_enemyPools[view->model_key()].push_back(std::move(mesh));
				}
			}
		};

		mark("document and rules");
		// The model files of the level's objects: load_models makes them a few per frame
		{
			const std::string levelDir = LevelDocument::directory_of(m_levelPath);
			std::set<std::string> files;
			const char* only = std::getenv("ZOMBIES_ONLY"); // debug filter
			m_prefabs.open(m_levelPath, doc);
			for (const auto& object : doc.objects)
			{
				// an object made from a prefab has the prefab's model
				std::string error;
				const std::string mesh = object.prefab.empty() ? object.mesh : m_prefabs.resolve(object, error).value("mesh", std::string());
				if (mesh.empty()) continue;
				if (!only || mesh.find(only) != std::string::npos) files.insert(levelDir + mesh);
			}
			// and the heroes' (the player is one of them, and so is someone escorted)
			const Json heroes = doc.extra.value("visuals", Json::object()).value("heroes", Json::object());
			for (auto it = heroes.begin(); it != heroes.end(); ++it)
			{
				if (it.value().is_string()) files.insert(levelDir + it.value().get<std::string>());
			}
			m_modelQueue.assign(files.begin(), files.end());
			m_modelsLoaded = 0;
			// All of them at once: they decode on the cache's workers while the loading screen draws (the previous level's
			// cache, if it handed one over, already has the files both use)
			if (!m_assetCache)
			{
				const unsigned cores = std::thread::hardware_concurrency();
				m_assetCache = m_carried.cache ? m_carried.cache : std::make_shared<AssetCache>(std::clamp(cores > 1 ? cores - 1 : 1u, 1u, 4u));
			}
			for (const auto& file : m_modelQueue)
			{
				m_prototypes[file] = m_assetCache->load<MeshComponent3D_Bgfx>(file);
			}
		}
		m_loadDoc = std::make_unique<LevelDocument>(std::move(doc));
		return true;
	}

	// The uploads of the models decoded so far, for at most 'budgetMs' (the graphics thread); true once all are loaded
	bool ZombiesLevelState::load_models(double budgetMs)
	{
		m_assetCache->pump(budgetMs);
		m_modelsLoaded = 0;
		for (const auto& file : m_modelQueue)
		{
			if (m_prototypes[file]->state() != AssetState::Loading) ++m_modelsLoaded;
		}
		if (m_modelsLoaded >= m_modelQueue.size())
		{
			LOG_INFO("[Load] %zu model files (%s)", m_modelQueue.size(), MeshComponent3D_Bgfx::take_load_stats().c_str());
			return true;
		}
		return false;
	}

	bool ZombiesLevelState::load_finish()
	{
		auto last = std::chrono::steady_clock::now();
		auto mark = [&last](const char* part)
		{
			const auto now = std::chrono::steady_clock::now();
			LOG_INFO("[Load] %s %.0f ms (%s)", part, std::chrono::duration<double, std::milli>(now - last).count(),
				MeshComponent3D_Bgfx::take_load_stats().c_str());
			last = now;
		};
		// a readable key for each object, the same ones the level's editor gives: the objects are known by them
		m_loadDoc->assign_keys();
		const LevelDocument& doc = *m_loadDoc;
		build_objects(doc);
		mark("objects");
		build_physics(doc);
		mark("physics");
		build_player(doc);
		mark("player");

		// The look of shooting: the bullet streak, the muzzle flash and the casings
		{
			const std::string levelDir = LevelDocument::directory_of(m_levelPath);
			auto role = [&](const char* name) -> std::string
			{
				auto found = doc.roles.find(name);
				return found != doc.roles.end() ? levelDir + found->second : std::string();
			};
			m_effects = std::make_unique<EffectsSystem>(m_graphics, [this](const std::string& file) -> std::unique_ptr<MeshComponent3D_Bgfx>
			{
				auto mesh = create_instance(file);
				return mesh->is_loaded() ? std::move(mesh) : nullptr;
			}, role("fx_bullet"), role("fx_muzzle"), role("fx_shell"), Pakal::RandomStreams::seed_for(m_random.seed(), "fx"));
			m_game->events.subscribe<ShotEvent>([this](const ShotEvent& shot) { if (m_effects) m_effects->on_shot(shot.muzzle, shot.direction); });
			m_game->events.subscribe<PropEffectEvent>([this](const PropEffectEvent& prop) { if (m_effects) m_effects->on_prop_effect(prop.at, prop.effect, prop.size); });
			m_game->events.subscribe<ImpactEvent>([this](const ImpactEvent& impact) { if (m_effects) m_effects->on_impact(impact.at, impact.direction, impact.kind); });
		}

		mark("effects");
		// Spawners of the level (the game's ZombieSpawner objects). Spawn orders come from the level scripts.
		m_spawns = std::make_unique<SpawnSystem>([this](const std::string& type, const b2Vec2& at) { return spawn_enemy(type, at); }, kPlayerRadius);
		for (const auto& spawner : doc.spawners)
		{
			m_spawns->add_point(b2Vec2(doc.meters(spawner.value("x", 0.0)), doc.meters(spawner.value("y", 0.0))));
		}
		// Models for the zombies alive at once (the scripts ask for up to 20, and bodies stay a moment). A level with bigger
		// waves asks for more ("enemyPool").
		{
			const int poolSize = std::max(1, doc.extra.value("enemyPool", kEnemyPoolSize));
			// A look with variants shares its zombies among them: each model needs fewer copies
			std::map<std::string, int> keys;
			auto want = [&](const CharacterViewComponent::Look& look)
			{
				const std::vector<std::string> variants = enemy_model_keys(look);
				const int size = look.pool > 0 ? look.pool : poolSize; // a rare type asks for fewer
				const int each = variants.size() > 1 ? size / static_cast<int>(variants.size()) + 4 : size;
				for (const auto& key : variants) keys[key] = std::max(keys[key], each);
			};
			want(m_enemyLook);
			for (const auto& entry : m_enemyLooks) want(entry.second);
			size_t models = 0;
			for (const auto& entry : keys)
			{
				const std::string& key = entry.first;
				if (key.empty()) continue;
				for (int i = 0; i < entry.second; ++i)
				{
					// every copy shares the model, loaded once (its own pose and clip)
					auto mesh = create_instance(key);
					mesh->set_scale(0.0f);
					m_enemyPools[key].push_back(std::move(mesh));
					++models;
				}
			}
			LOG_INFO("[ZombiesLevel] %zu spawners, %zu models in the zombie pools (%zu kinds)", m_spawns->point_count(), models, keys.size());
		}
		mark("zombie pools");

		start_objective_scripts(doc);
		// Debug: more spawn orders on top of the script's, "type:alive:cooldownMs,..." (ZOMBIES_SPAWN=25016:2:3000)
		if (const char* extra = std::getenv("ZOMBIES_SPAWN"))
		{
			std::string list = extra;
			for (size_t start = 0; start < list.size();)
			{
				const size_t comma = std::min(list.find(',', start), list.size());
				int type = 0, alive = 0;
				float cooldown = 1000.0f;
				if (std::sscanf(list.substr(start, comma - start).c_str(), "%d:%d:%f", &type, &alive, &cooldown) >= 2)
				{
					m_spawns->set_order(std::to_string(type), alive, cooldown);
				}
				start = comma + 1;
			}
		}

		// The light and the look of the picture: the level's "lighting" and "post" sections (LevelLook)
		{
			const LevelLook look = LevelLook::read(doc.lighting, doc.extra.value("post", Json::object()));
			look.apply(*m_graphics);
			m_gloss = look.gloss;
			m_levelShadows = look.shadows;
			m_levelPost = look.post;
			if (look.post.enabled) LOG_INFO("[ZombiesLevel] post-process on (grading, tilt-shift %.0f px, occlusion %.2f)", look.post.tiltBlur, look.post.aoStrength);
		}

		// The baked ambient light (gi.json + gi.bin next to the level, from build_downtown.py); ZOMBIES_GI=off skips it
		{
			const std::string levelDir = LevelDocument::directory_of(m_levelPath);
			std::ifstream meta(levelDir + "gi.json");
			const char* env = std::getenv("ZOMBIES_GI");
			if (meta && !(env && std::string(env) == "off"))
			{
				const Json gi = Json::parse(meta, nullptr, false);
				std::ifstream data(levelDir + "gi.bin", std::ios::binary);
				if (gi.is_object() && data)
				{
					const auto dims = gi.value("dims", std::vector<int>());
					const auto origin = gi.value("origin", std::vector<float>());
					const auto size = gi.value("size", std::vector<float>());
					if (dims.size() == 3 && origin.size() == 3 && size.size() == 3)
					{
						std::vector<uint8_t> bytes(static_cast<size_t>(dims[0]) * dims[1] * dims[2] * 4);
						data.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
						// Level axes (x east, y north, height) to render space (x east, y up, z south): z = -y
						const float renderOrigin[3] = { origin[0], origin[2], -origin[1] };
						const float renderSize[3] = { size[0], size[2], -size[1] };
						m_graphics->set_gi_volume(bytes.data(), uint16_t(dims[0]), uint16_t(dims[1]), uint16_t(dims[2]), renderOrigin, renderSize, 0.75f);
					}
				}
			}
		}

		// The light baked per texel for the floor and the buildings (lightmap atlas next to the level, from the level builder);
		// ZOMBIES_LIGHTMAP=off leaves them to the light probes
		{
			const Json lightmap = doc.extra.value("lightmap", Json::object());
			const char* env = std::getenv("ZOMBIES_LIGHTMAP");
			if (lightmap.contains("texture") && !(env && std::string(env) == "off"))
			{
				m_graphics->set_lightmap(LevelDocument::directory_of(m_levelPath) + lightmap.value("texture", std::string()),
					lightmap.value("shadowStrength", 0.75f));
			}
		}

		// With the light baked (light probes, a lightmap) the ambient occlusion is in it: the screen's (SSAO) runs only at ultra,
		// where there is power to spare; ZOMBIES_AO sets it anyway, and the dev panel can turn it on
		if ((m_graphics->has_gi() || m_graphics->has_lightmap()) && !std::getenv("ZOMBIES_AO")
			&& m_graphics->get_quality() != BgfxGraphicsSystem::Quality::Ultra && m_levelPost.aoStrength > 0.0f)
		{
			m_devGraphics.ao = false;
			apply_dev_graphics();
			LOG_INFO("[ZombiesLevel] the occlusion is baked: no screen-space occlusion below ultra");
		}

		// Camera: follows the player from above; the first update places it
		m_camera = std::make_unique<CameraComponent3D_Bgfx>(m_graphics);
		m_camera->set_perspective(m_rig->settings().fovY, 0.5f, 5000.0f);
		m_camera->set_viewport(0, 0, static_cast<int>(m_graphics->get_width()), static_cast<int>(m_graphics->get_height()));
		m_graphics->set_active_camera(m_camera.get());
		update_views(0.0f);
		mark("scripts, light, GI, post, camera");

		LOG_INFO("[ZombiesLevel] '%s' loaded: %zu actors, %zu models, walkable grid %dx%d", m_levelPath.c_str(),
			m_game->actors().size(), m_levelObjects.instances().size(), m_nav->get_width(), m_nav->get_height());
		LOG_INFO("[ZombiesLevel] controls: move WASD, aim with the mouse (or IJKL), fire with the left button (or Space)");
#if !defined(__ANDROID__)
		// the level's document stays for the editor (the dev panel's "Edit level"), with each object's model
		m_edit = std::make_unique<LevelEditPanel>(m_levelPath, std::move(m_loadDoc), [](MeshComponent3D_Bgfx& mesh, const LevelObject& object)
		{
			mesh.set_matrix(placement_matrix(object).data());
		});
		for (size_t i = 0; i < m_levelObjects.instances().size(); ++i) m_edit->set_mesh(i, m_levelObjects.mesh_at(i));
		m_edit->set_live_reload(m_carried.edit.keep ? m_carried.edit.liveReload : std::getenv("ZOMBIES_LIVE_RELOAD") != nullptr);
		m_edit->set_editing(m_carried.edit.keep ? m_carried.edit.editing : std::getenv("ZOMBIES_EDIT") != nullptr);
		if (m_carried.edit.keep)
		{
			m_edit->select(m_carried.edit.selected);
			m_editFocus = m_carried.edit.focus;
			m_wasEditing = m_carried.edit.editing;   // the camera stays where the editor had it
		}
		else if (const char* select = std::getenv("ZOMBIES_EDIT_SELECT"))
		{
			m_edit->select(select);   // debug: an object selected from the start (its gizmo in captures)
		}
		if (m_edit->editing() && m_graphics) m_graphics->get_debug_ui().set_visible(true);
		std::error_code timeError;
		m_levelWriteTime = std::filesystem::last_write_time(m_levelPath, timeError);
#endif
		m_loadDoc.reset();
		// the previous level's models this one does not use go now (here, on the graphics thread)
		m_carried = LevelCarry();
		m_assetCache->trim();
		return true;
	}

	void ZombiesLevelState::build_objects(const LevelDocument& doc)
	{
		const std::string levelDir = LevelDocument::directory_of(m_levelPath);
		std::map<int, const PropRules*> propRules;
		for (const auto& rules : m_rules.props)
		{
			propRules[rules.id] = &rules;
		}
		const std::string giftName = "GiftBox";
		m_schema = ComponentSchema();
		ComponentSchema::add_topdown_kinds(m_schema);
		const char* only = std::getenv("ZOMBIES_ONLY");   // debug filter: only the models whose file has this
		m_levelObjects.setup(levelDir, doc.unitsPerMeter, &m_prefabs, &m_schema, [this, only](const std::string& file) -> std::unique_ptr<MeshComponent3D_Bgfx>
		{
			if (only && file.find(only) == std::string::npos) return nullptr;
			return create_instance(file);
		});
		// What the converted levels' objects are: buildings and trees fade, an object with a game id is a prop (breakable
		// when its rules say), the gift box waits hidden. An object with components (a prefab's) is LevelObjects' own. Kept: an
		// object added while the level runs (live reload) goes through it too.
		const double unitsPerMeter = doc.unitsPerMeter;
		m_objectHandle = [this, propRules, giftName, unitsPerMeter](size_t, const LevelObject& object, const Json& resolved, MeshComponent3D_Bgfx& mesh,
			const std::array<float, 16>& placement, uintptr_t& made) -> bool
		{
			// Buildings, trees and anything else tall and wide fade when they hide the player (OccluderFade: tested in the model's frame)
			float lo[3], hi[3];
			mesh.get_bounds(lo, hi);
			if (OccluderFade::may_hide(object.name, placement.data(), lo, hi))
			{
				OccluderFade::Occluder occluder = OccluderFade::make(&mesh, placement.data(), lo, hi);
				// Trees fade by dithering (solid, leaving pixels out): blending their many layers of leaves cost 10-15 ms. Buildings
				// keep blending: dithered, a building that fills the screen cost as much (the discard turns off the early depth
				// test). ZOMBIES_FADE=dither (debug) dithers them too, to compare.
				static const bool ditherBuildings = std::getenv("ZOMBIES_FADE") && std::string(std::getenv("ZOMBIES_FADE")) == "dither";
				if (object.name.rfind("Tree", 0) == 0)
				{
					occluder.faded = 0.4f;
					mesh.set_fade_dither(true);
				}
				else if (ditherBuildings)
				{
					occluder.faded = 0.3f;
					mesh.set_fade_dither(true);
				}
				m_occluders.add(occluder);
			}
			if (!resolved.value("components", Json::object()).empty()) return false;
			if (object.id < 0) return true;   // scenery
			Actor& prop = m_game->add_actor(ActorKind::Prop, object.name);
			prop.gameId = object.id;
			prop.position = b2Vec2(static_cast<float>(object.x / unitsPerMeter), static_cast<float>(object.y / unitsPerMeter));
			PropComponent& view = prop.add<PropComponent>(std::make_unique<PropMesh>(&mesh, placement));
			auto rules = propRules.find(object.id);
			if (rules != propRules.end() && rules->second->health > 0.0f)
			{
				DestructibleComponent& breakable = prop.add<DestructibleComponent>(*rules->second);
				float lo[3], hi[3];
				mesh.get_bounds(lo, hi);
				const float scale = static_cast<float>(object.scale);
				breakable.set_size(std::clamp(std::max(hi[0] - lo[0], hi[2] - lo[2]) * scale, 0.3f, 6.0f));
				// its footprint on the floor (level x, y: render x, -z), where zombies stand to hit it
				b2Vec2 low(1e9f, 1e9f), high(-1e9f, -1e9f);
				for (int corner = 0; corner < 8; ++corner)
				{
					const float px = (corner & 1) ? hi[0] : lo[0], py = (corner & 2) ? hi[1] : lo[1], pz = (corner & 4) ? hi[2] : lo[2];
					const float wx = placement[0] * px + placement[4] * py + placement[8] * pz + placement[12];
					const float wz = placement[2] * px + placement[6] * py + placement[10] * pz + placement[14];
					low = b2Vec2(std::min(low.x, wx), std::min(low.y, -wz));
					high = b2Vec2(std::max(high.x, wx), std::max(high.y, -wz));
				}
				breakable.set_box(low, high);
				if (rules->second->repairable) m_game->barricades.push_back(prop.id);
				if (rules->second->objective) m_game->objective = prop.id;
			}
			if (object.name == giftName)
			{
				// The gift waits hidden until the script's EnableRandomObject
				prop.add<GiftBoxComponent>();
				view.show(false);
				view.enable(false);
			}
			m_actorsByGameId[object.id].push_back(prop.id);
			made = prop.id;
			return true;
		};
		m_levelObjects.build(doc, m_game.get(), m_objectHandle);
		// the props made from components: scripts find them by game id too, and barricades and the objective are the game's
		for (const auto& instance : m_levelObjects.instances())
		{
			if (!instance.ownActor) continue;
			const Actor* prop = m_game->find(instance.actor);
			if (!prop) continue;
			if (prop->gameId >= 0) m_actorsByGameId[prop->gameId].push_back(prop->id);
			if (const auto* breakable = prop->get<DestructibleComponent>())
			{
				if (breakable->rules().repairable) m_game->barricades.push_back(prop->id);
				if (breakable->rules().objective) m_game->objective = prop->id;
			}
		}
		for (const auto& unknown : m_levelObjects.unknown()) LOG_WARNING("[ZombiesLevel] unknown component of %s", unknown.c_str());
		LOG_INFO("[ZombiesLevel] %zu models placed (%zu failed), %zu props by id", m_levelObjects.instances().size() - m_levelObjects.failed(),
			m_levelObjects.failed(), m_actorsByGameId.size());
	}

	void ZombiesLevelState::build_physics(const LevelDocument& doc)
	{
		const LevelPhysicsCount count = build_level_physics(doc, *m_world, *m_nav, m_game->layers, kPlayerRadius,
			[this](int gameId) -> uintptr_t
			{
				auto owner = m_actorsByGameId.find(gameId);
				return owner != m_actorsByGameId.end() ? owner->second.front() : 0;
			},
			[this](uintptr_t actor) { return std::find(m_game->barricades.begin(), m_game->barricades.end(), actor) != m_game->barricades.end(); });
		LOG_INFO("[ZombiesLevel] physics: %zu prop walls, %zu collision polygons", count.walls, count.outlines);
	}

	void ZombiesLevelState::build_player(const LevelDocument& doc)
	{
		const std::string levelDir = LevelDocument::directory_of(m_levelPath);
		auto roleFile = [&](const char* role) -> std::string
		{
			auto found = doc.roles.find(role);
			return found != doc.roles.end() ? levelDir + found->second : std::string();
		};
		m_playerModel = roleFile("player");
		m_enemyModel = roleFile("enemy");

		// How they look and move: the level's "visuals" over the defaults
		const Json visuals = doc.extra.value("visuals", Json::object());
		m_playerLook = CharacterViewComponent::Look::from_json(visuals.value("player", Json()), default_player_look());
		// The hero the player picked (Session's profile; ZOMBIES_HERO=<id> for tests): the kit's survivors share the look
		std::string hero = m_missionId.empty() ? std::string() : Session::get().profile.hero;
		if (const char* forced = std::getenv("ZOMBIES_HERO")) hero = forced;
		const Json heroes = visuals.value("heroes", Json::object());
		for (auto it = heroes.begin(); it != heroes.end(); ++it) m_heroModels[it.key()] = levelDir + it.value().get<std::string>();
		if (!hero.empty() && heroes.contains(hero))
		{
			m_playerModel = levelDir + heroes[hero].get<std::string>();
			LOG_INFO("[ZombiesLevel] playing as %s", hero.c_str());
		}
		const Json enemies = visuals.value("enemy", Json::object());
		m_enemyLook = CharacterViewComponent::Look::from_json(enemies.value("default", Json()), default_enemy_look());
		if (enemies.is_object())
		{
			for (auto it = enemies.begin(); it != enemies.end(); ++it)
			{
				if (it.key() != "default") m_enemyLooks[it.key()] = CharacterViewComponent::Look::from_json(it.value(), m_enemyLook);
			}
		}

		// where the level starts it, or where it was before a reload of the edited level
		const b2Vec2 spawn = m_carried.edit.keep ? m_carried.edit.playerAt : b2Vec2(doc.meters(doc.spawn[0]), doc.meters(doc.spawn[1]));
		Actor& player = m_game->add_actor(ActorKind::Character, "Player", m_game->survivors);
		player.position = spawn;
		player.yaw = actor_yaw(0.0f, -1.0f);
		player.add<StatsComponent>();   // the boosts change its speed, damage and defence
		player.add<HealthComponent>(m_rules.player.health, m_rules.player.invincibleAfterHitMs / 1000.0f);
		player.add<ConsumableComponent>();
		player.add<PlayerControlComponent>();
		player.add<CharacterBodyComponent>(*m_game, spawn, kPlayerRadius);
		player.add<WeaponComponent>(m_rules.gun);
		if (!m_playerModel.empty())
		{
			CharacterViewComponent& view = player.add<CharacterViewComponent>(create_instance(m_playerModel), m_playerLook);
			if (m_playerLook.hasHeld && !m_playerLook.held.mesh.empty())
			{
				view.set_held(create_instance(levelDir + m_playerLook.held.mesh));
			}
			for (const auto& weapon : m_playerLook.weapons)
			{
				view.add_weapon(weapon.first, create_instance(levelDir + weapon.second.mesh), weapon.second);
			}
			if (!m_missionId.empty())
			{
				const Session& session = Session::get();
				if (const WeaponDef* equipped = session.campaign.weapon(session.profile.equipped))
				{
					view.set_base_weapon(equipped->consumable);
				}
			}
		}
		m_game->add_player(player);
		m_playerId = player.id;
		if (const char* give = std::getenv("ZOMBIES_GIVE")) // debug: starts with this consumable (by game id), for a long time
		{
			auto found = m_rules.consumables.find(std::atoi(give));
			if (found != m_rules.consumables.end())
			{
				static ConsumableRules longLasting;
				longLasting = found->second;
				longLasting.timerMs = 1.0e9f;
				player.get<ConsumableComponent>()->apply(*m_game, longLasting);
			}
		}
	}

	std::string ZombiesLevelState::enemy_model_key(const CharacterViewComponent::Look& look) const
	{
		return look.model.empty() ? m_enemyModel : LevelDocument::directory_of(m_levelPath) + look.model;
	}

	std::vector<std::string> ZombiesLevelState::enemy_model_keys(const CharacterViewComponent::Look& look) const
	{
		std::vector<std::string> keys = { enemy_model_key(look) };
		for (const auto& variant : look.variants)
		{
			const std::string key = LevelDocument::directory_of(m_levelPath) + variant;
			if (std::find(keys.begin(), keys.end(), key) == keys.end()) keys.push_back(key);
		}
		return keys;
	}

	uintptr_t ZombiesLevelState::spawn_enemy(const std::string& type, const b2Vec2& at)
	{
		auto lookIt = m_enemyLooks.find(type);
		const CharacterViewComponent::Look& look = lookIt != m_enemyLooks.end() ? lookIt->second : m_enemyLook;
		// One of the look's models at random; the next one when its copies are all in use
		const std::vector<std::string> keys = enemy_model_keys(look);
		const size_t first = std::min(keys.size() - 1, static_cast<size_t>(m_game->random01() * static_cast<float>(keys.size())));
		std::string key;
		auto pool = m_enemyPools.end();
		for (size_t i = 0; i < keys.size() && (pool == m_enemyPools.end() || pool->second.empty()); ++i)
		{
			key = keys[(first + i) % keys.size()];
			pool = m_enemyPools.find(key);
		}
		if (pool == m_enemyPools.end() || pool->second.empty())
		{
			return 0; // every model is in use: the order tries again after its cooldown
		}
		const EnemyRules& rules = m_rules.enemy(type);
		Actor& enemy = m_game->add_actor(ActorKind::Character, rules.name.empty() ? type : rules.name, m_game->undead);
		enemy.gameId = std::atoi(type.c_str());
		enemy.position = at;
		enemy.add<EnemyBrainComponent>(*m_game, type, at, enemy.id);
		auto mesh = std::move(pool->second.back());
		pool->second.pop_back();
		CharacterViewComponent& view = enemy.add<CharacterViewComponent>(std::move(mesh), look);
		view.set_model_key(key);
		static int logged = 0;
		if (logged < 8 && keys.size() > 1)
		{
			++logged;
			LOG_INFO("[ZombiesLevel] zombie %s wears %s", type.c_str(), key.substr(key.find_last_of("/\\") + 1).c_str());
		}
		return enemy.id;
	}
}
