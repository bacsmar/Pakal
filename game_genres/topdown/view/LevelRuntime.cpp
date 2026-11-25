///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelRuntime.cpp
// Purpose: What every TopDown level state does around its game - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/view/LevelRuntime.h"
#include "topdown/CameraInput.h"
#include "topdown/view/GrassFromPlants.h"
#include "topdown/ShadowProxies.h"
#include "topdown/TopDownGame.h"
#include "topdown/view/LevelPlacement.h"
#include "topdown/view/OccluderFade.h"
#include "topdown/view/ParticleEffects.h"
#include "topdown/view/RenderSpace.h"
#include "LevelLayers.h"
#include "ResourceManager.h"

#include "Engine.h"
#include "LogMgr.h"
#include "OSManager.h"
#include "Profiler.h"
#include "bgfx/BgfxGraphicsSystem.h"
#include "bgfx/CameraComponent3D_Bgfx.h"
#include "bgfx/MeshComponent3D_Bgfx.h"

#include <algorithm>
#include <cstring>
#include <chrono>
#include <thread>
#include "math/Vec2.h"

namespace Pakal::TopDown
{
	LevelRuntime::LevelRuntime(Pakal::Engine& engine) :
		m_engine(engine),
		m_graphics(static_cast<Pakal::BgfxGraphicsSystem*>(engine.graphics_system()))
	{
		const unsigned cores = std::thread::hardware_concurrency();
		m_assets = std::make_shared<Pakal::AssetCache>(std::clamp(cores > 1 ? cores - 1 : 1u, 1u, 4u));
	}

	LevelRuntime::~LevelRuntime()
	{
		end();
	}

	bool LevelRuntime::begin(const Settings& settings)
	{
		if (!m_graphics)
		{
			LOG_ERROR("[LevelRuntime] no graphics system");
			return false;
		}
		m_settings = settings;
		m_random = Pakal::RandomStreams(settings.seed);
		m_rig = std::make_unique<CameraRig>(settings.camera, m_random["shake"]);
		m_particles = std::make_unique<ParticleEffects>(m_graphics->particle_renderer(), Pakal::RandomStreams::seed_for(settings.seed, "particles"));
		InputMapper::Settings input = settings.input;
		input.cameraAngle = camera_input_angle(settings.camera.side, settings.camera.back);
		m_input = std::make_unique<InputMapper>(input);
		Pakal::OSManager* os = m_engine.os_manager();
		m_input->listen(os ? os->get_input_manager() : nullptr, os, [](int, int) { return false; });
		on_graphics([this]()
		{
			m_camera = std::make_unique<Pakal::CameraComponent3D_Bgfx>(m_graphics);
			m_camera->set_perspective(m_settings.camera.fovY, m_settings.nearPlane, m_settings.farPlane);
			m_camera->set_viewport(0, 0, static_cast<int>(m_graphics->get_width()), static_cast<int>(m_graphics->get_height()));
			m_graphics->set_active_camera(m_camera.get());
		});
		return true;
	}

	void LevelRuntime::on_graphics(const std::function<void()>& work)
	{
		auto task = m_graphics->execute_block(work);
		if (task) task->wait();
	}

	void LevelRuntime::prefetch(const std::vector<std::string>& files)
	{
		for (const auto& file : files)
		{
			if (!m_prototypes[file]) m_prototypes[file] = m_assets->load<Pakal::MeshComponent3D_Bgfx>(file);
		}
	}

	std::unique_ptr<Pakal::MeshComponent3D_Bgfx> LevelRuntime::model(const std::string& file)
	{
		Pakal::AssetHandle<Pakal::MeshComponent3D_Bgfx>& prototype = m_prototypes[file];
		if (!prototype) prototype = m_assets->load<Pakal::MeshComponent3D_Bgfx>(file);
		while (prototype->state() == Pakal::AssetState::Loading)
		{
			m_assets->pump(20.0);   // this is the graphics thread: the uploads happen here
			if (prototype->state() == Pakal::AssetState::Loading) std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		const Pakal::MeshComponent3D_Bgfx* loaded = prototype->get();
		if (!loaded || !loaded->is_loaded())
		{
			LOG_WARNING("[LevelRuntime] model not loaded: %s", file.c_str());
			return nullptr;
		}
		auto instance = std::make_unique<Pakal::MeshComponent3D_Bgfx>(m_graphics);
		instance->share_from(*loaded);
		return instance;
	}

	std::unique_ptr<Pakal::MeshComponent3D_Bgfx> LevelRuntime::character(const Pakal::CharacterRecipe& recipe)
	{
		const std::string key = recipe.to_json();
		auto found = m_characters.find(key);
		if (found == m_characters.end())
		{
			auto prototype = std::make_unique<Pakal::MeshComponent3D_Bgfx>(m_graphics);
			if (!prototype->load_character(recipe) || !prototype->is_loaded()) prototype.reset();   // (load_character logs why)
			found = m_characters.emplace(key, std::move(prototype)).first;
		}
		if (!found->second) return nullptr;
		auto instance = std::make_unique<Pakal::MeshComponent3D_Bgfx>(m_graphics);
		instance->share_from(*found->second);
		return instance;
	}

	std::unique_ptr<Pakal::MeshComponent3D_Bgfx> LevelRuntime::character_model(const CharacterViewComponent::Look& look, size_t variant)
	{
		if (!look.recipes.empty()) return character(look.recipes[variant % look.recipes.size()]);
		if (variant == 0 || look.variants.empty()) return model(look.model);
		return model(look.variants[(variant - 1) % look.variants.size()]);
	}

	std::unique_ptr<Pakal::MeshComponent3D_Bgfx> LevelRuntime::model_if_ready(const std::string& file, bool& failed)
	{
		failed = false;
		Pakal::AssetHandle<Pakal::MeshComponent3D_Bgfx>& prototype = m_prototypes[file];
		if (!prototype) prototype = m_assets->load<Pakal::MeshComponent3D_Bgfx>(file);
		if (prototype->state() == Pakal::AssetState::Loading)
		{
			m_assets->pump(4.0);   // a little of the uploads each frame
			if (prototype->state() == Pakal::AssetState::Loading) return nullptr;
		}
		const Pakal::MeshComponent3D_Bgfx* loaded = prototype->get();
		if (!loaded || !loaded->is_loaded())
		{
			failed = true;
			return nullptr;
		}
		auto instance = std::make_unique<Pakal::MeshComponent3D_Bgfx>(m_graphics);
		instance->share_from(*loaded);
		return instance;
	}

	void LevelRuntime::apply_look(const SceneLook& look)
	{
		look.apply(*m_graphics);
	}

	void LevelRuntime::set_wind(float bend, float speed)
	{
		Pakal::MeshComponent3D_Bgfx::set_wind(bend, speed);
	}

	void LevelRuntime::set_draw_distance(float perMeter, float minimum)
	{
		Pakal::MeshComponent3D_Bgfx::set_draw_distance(perMeter, minimum);
	}

	void LevelRuntime::place_scenery(const nlohmann::json& items, const std::string& folder)
	{
		if (!items.is_array()) return;
		for (const auto& item : items)
		{
			auto mesh = model(folder + item.value("model", std::string()));
			if (!mesh) continue;
			const nlohmann::json at = item.value("at", nlohmann::json::array({ 0.0f, 0.0f }));
			const float yaw = item.value("yaw", 0.0f) * 0.0174532925f;
			// centered on its point, standing on the floor
			float low[3], high[3];
			mesh->get_bounds(low, high);
			const tmath::vector3df p = floor_point(Pakal::Vec2(at[0].get<float>(), at[1].get<float>()));
			const float cx = 0.5f * (low[0] + high[0]), cz = 0.5f * (low[2] + high[2]);
			const float c = std::cos(yaw), s = std::sin(yaw);
			mesh->set_rotation(0.0f, yaw, 0.0f);
			mesh->set_position(p.x - (c * cx + s * cz), p.y - low[1], p.z - (-s * cx + c * cz));
			m_scenery.push_back(std::move(mesh));
		}
	}

	bool LevelRuntime::place_level(const std::string& levelPath, const std::function<bool(const Pakal::LevelObject&)>& keep, const LevelScenery& scenery)
	{
		// levels, their bases and their layers through the resource manager (a folder, a zip, the APK), unless the game set
		// its own reader: the disk alone does not see an APK's assets
		if (!Pakal::LevelDocument::has_file_reader())
		{
			Pakal::LevelDocument::set_file_reader([](const std::string& path, std::string& text)
			{
				bool found = false;
				text = ResourceMgr.read_text(path, &found);
				return found;
			});
		}
		Pakal::LevelDocument doc;
		std::string error;
		if (!Pakal::load_level(levelPath, doc, error))
		{
			LOG_ERROR("[LevelRuntime] %s", error.c_str());
			return false;
		}
		const std::string levelDir = Pakal::LevelDocument::directory_of(levelPath);
		m_levelDir = levelDir;
		m_levelObjects.clear();
		if (!m_occluders) m_occluders = std::make_unique<OccluderFade>();
		size_t placed = 0, failed = 0, proxies = 0;
		const float turnC = std::cos(scenery.yaw), turnS = std::sin(scenery.yaw);
		// rotation_y(-yaw), column-major
		const Mat4 unturn{ turnC, 0, turnS, 0, 0, 1, 0, 0, -turnS, 0, turnC, 0, 0, 0, 0, 1 };
		ShadowProxies shadowProxies(scenery.shadowProxy);
		GrassFromPlants grass(scenery.grass);
		for (const Pakal::LevelObject& object : doc.objects)
		{
			if (keep && !keep(object)) continue;
			if (object.mesh.empty())
			{
				++failed;   // (made from a prefab: not drawn yet)
				continue;
			}
			Mat4 placement = level_placement(object, doc.unitsPerMeter);
			// moved by -origin (level x is render x, level y is render -z), then turned by -yaw about the vertical (a turn
			// counterclockwise from above in the level is rotation_y of the same angle in render space)
			placement[12] -= scenery.origin.x;
			placement[14] += scenery.origin.y;
			if (scenery.yaw != 0.0f) placement = mat4_mul(unturn, placement);
			// a grass model the field replaces (and no keyed object, which a game may move or swap): its place goes to the
			// field, and no model is drawn
			if (!grass.empty() && object.key.empty() && (grass.replaces(object.name) || grass.replaces(object.mesh)))
			{
				grass.add(placement[12], placement[13], placement[14]);
				continue;
			}
			auto mesh = model(levelDir + object.mesh);
			if (!mesh || !mesh->is_loaded())
			{
				++failed;
				continue;
			}
			mesh->set_matrix(placement.data());
			if (object.lightmap[0] > 0.0) mesh->set_lightmap_rect(float(object.lightmap[0]), float(object.lightmap[1]), float(object.lightmap[2]));
			mesh->set_shadow_baked(object.baked);
			// a building in colors of its own: its walls and roof painted over its atlas (the level builder's paint.py), or
			// all of it tinted (a landmark's letters)
			if (object.paint.is_object())
			{
				auto rgb = [&object](const char* key, float out[3])
				{
					const Pakal::Json c = object.paint.value(key, Pakal::Json::array());
					for (int i = 0; i < 3; ++i) out[i] = c.is_array() && c.size() > size_t(i) && c[i].is_number() ? c[i].get<float>() : 1.0f;
				};
				if (object.paint.contains("atlas"))
				{
					float wall[3], roof[3];
					rgb("wall", wall);
					rgb("roof", roof);
					mesh->set_paint(object.paint.value("atlas", std::string()), levelDir + object.paint.value("mask", std::string()), wall, roof);
				}
				if (object.paint.contains("tint"))
				{
					float tint[3];
					rgb("tint", tint);
					mesh->set_color(tint[0], tint[1], tint[2]);
				}
			}
			// buildings, trees and anything tall and wide fade while they hide a character (fade_occluders)
			float lo[3], hi[3];
			mesh->get_bounds(lo, hi);
			if (OccluderFade::may_hide(object.name, placement.data(), lo, hi))
			{
				// a tree's box is mostly empty (a round crown on a thin trunk): it is tested by the middle of its box across
				const bool tree = object.name.rfind("Tree", 0) == 0;
				if (tree)
				{
					for (int axis : { 0, 2 })
					{
						const float middle = 0.5f * (lo[axis] + hi[axis]), half = 0.5f * (hi[axis] - lo[axis]) * 0.55f;
						lo[axis] = middle - half;
						hi[axis] = middle + half;
					}
				}
				OccluderFade::Occluder occluder = OccluderFade::make(mesh.get(), placement.data(), lo, hi);
				if (tree || scenery.buildingFade == "dither")
				{
					occluder.faded = tree ? 0.4f : 0.3f;
					mesh->set_fade_dither(true);
				}
				else if (scenery.buildingFade == "cut")
				{
					occluder.faded = scenery.floor;
					mesh->set_fade_cut(scenery.keep, scenery.rim, scenery.rimColor, scenery.floor);
				}
				m_occluders->add(occluder);
			}
			// its shadow cast by its proxy (a tree's leaf cards drawn in the sun's map were most of its cost)
			std::unique_ptr<Pakal::MeshComponent3D_Bgfx> shadow;
			if (const std::string proxyFile = shadowProxies.proxy_for(levelDir + object.mesh); !proxyFile.empty())
			{
				shadow = model(proxyFile);
				if (shadow && shadow->is_loaded())
				{
					shadow->set_matrix(placement.data());
					cast_shadow_through(*mesh, *shadow);
				}
				else
				{
					shadow.reset();
				}
			}
			LevelEntry entry{ m_scenery.size(), placement, placement };
			m_scenery.push_back(std::move(mesh));
			if (shadow)
			{
				entry.proxy = m_scenery.size();
				m_scenery.push_back(std::move(shadow));
				++proxies;
			}
			if (!object.key.empty()) m_levelObjects[object.key] = entry;
			++placed;
		}
		if (failed) LOG_WARNING("[LevelRuntime] %s: %zu objects placed, %zu not drawn", levelPath.c_str(), placed, failed);
		if (proxies) LOG_INFO("[LevelRuntime] %s: %zu models cast their shadow through a proxy", levelPath.c_str(), proxies);
		// the grass models the field replaced: one field of blades over their places
		if (m_grassField)
		{
			m_graphics->destroy_grass(m_grassField);
			m_grassField = 0;
		}
		if (grass.plants() > 0)
		{
			Pakal::GraphicsSystem::GrassField field = grass.field();
			const float width = field.maxX - field.minX, depth = field.maxZ - field.minZ;
			m_grassField = m_graphics->create_grass(std::move(field));
			LOG_INFO("[LevelRuntime] %s: %zu grass models drawn as one field of blades over %.0f x %.0f m", levelPath.c_str(),
				grass.plants(), width, depth);
		}

		// the height of the floor as it is drawn (tools/levelbuilder/floor.py): a byte a cell, centimeters plus 128, rows
		// from the south edge; between cells the height blends (a curb is a short ramp, not a step)
		m_floorHeight = nullptr;
		{
			const Pakal::Json floor = doc.extra.value("floor", Pakal::Json::object());
			const Pakal::Json bounds = floor.value("bounds", Pakal::Json::array());
			const auto data = floor.contains("file") ? ResourceMgr.read_bytes(levelDir + floor.value("file", std::string())) : nullptr;
			const int columns = floor.value("columns", 0), rows = floor.value("rows", 0);
			if (data && bounds.is_array() && bounds.size() == 4 && columns > 1 && rows > 1 && data->size() >= size_t(columns) * size_t(rows))
			{
				const float x0 = bounds[0].get<float>(), y0 = bounds[1].get<float>(), cell = floor.value("cell", 0.5f);
				// (asked in the game's frame: the level's point is origin + R(yaw) p)
				m_floorHeight = [data, x0, y0, cell, columns, rows, origin = scenery.origin, turnC, turnS](const Pakal::Vec2& p)
				{
					const Pakal::Vec2 at(origin.x + turnC * p.x - turnS * p.y, origin.y + turnS * p.x + turnC * p.y);
					const float fx = std::clamp((at.x - x0) / cell, 0.0f, float(columns - 1)), fy = std::clamp((at.y - y0) / cell, 0.0f, float(rows - 1));
					const int cx = std::min(int(fx), columns - 2), cy = std::min(int(fy), rows - 2);
					auto height = [&](int x, int y) { return (float((*data)[size_t(y) * size_t(columns) + size_t(x)]) - 128.0f) * 0.01f; };
					const float tx = std::clamp(fx - float(cx), 0.0f, 1.0f), ty = std::clamp(fy - float(cy), 0.0f, 1.0f);
					const float south = height(cx, cy) + (height(cx + 1, cy) - height(cx, cy)) * tx;
					const float north = height(cx, cy + 1) + (height(cx + 1, cy + 1) - height(cx, cy + 1)) * tx;
					return south + (north - south) * ty;
				};
			}
		}
		CharacterViewComponent::set_floor(m_floorHeight);
		if (m_particles) m_particles->set_floor(m_floorHeight);

		// the light baked per texel for the floor and the buildings (the level builder's lightmap atlas)
		const Pakal::Json lightmap = doc.extra.value("lightmap", Pakal::Json::object());
		if (scenery.lightmap && lightmap.contains("texture"))
		{
			m_lightmapSet = m_graphics->set_lightmap(levelDir + lightmap.value("texture", std::string()),
				lightmap.value("shadowStrength", 0.75f), lightmap.value("relative", false));
		}

		// the baked ambient light (light probes): a box of the level, looked up through the same move and turn
		if (scenery.gi)
		{
			std::string name = "gi";
			if (!scenery.giLook.empty() && ResourceMgr.exists(levelDir + "gi_" + scenery.giLook + ".json")) name = "gi_" + scenery.giLook;
			bool found = false;
			const Pakal::Json gi = Pakal::Json::parse(ResourceMgr.read_text(levelDir + name + ".json", &found), nullptr, false);
			const auto data = found ? ResourceMgr.read_bytes(levelDir + name + ".bin") : nullptr;
			if (found && gi.is_object() && data)
			{
				const auto dims = gi.value("dims", std::vector<int>());
				const auto corner = gi.value("origin", std::vector<float>());
				const auto size = gi.value("size", std::vector<float>());
				if (dims.size() == 3 && corner.size() == 3 && size.size() == 3 && dims[0] > 0 && dims[1] > 0 && dims[2] > 0)
				{
					std::vector<uint8_t> bytes(static_cast<size_t>(dims[0]) * dims[1] * dims[2] * 4);
					std::memcpy(bytes.data(), data->data(), std::min(bytes.size(), data->size()));
					// level axes (x east, y north, height) to render space (z = -y); a world point is turned by yaw, then the
					// origin's offset is taken from the box's corner
					const float renderCorner[3] = { corner[0] - scenery.origin.x, corner[2], -corner[1] + scenery.origin.y };
					const float renderSize[3] = { size[0], size[2], -size[1] };
					m_graphics->set_gi_volume(bytes.data(), uint16_t(dims[0]), uint16_t(dims[1]), uint16_t(dims[2]), renderCorner, renderSize,
						scenery.giStrength, scenery.yaw);
					m_giSet = true;
					LOG_INFO("[LevelRuntime] light probes %s%s.json: %dx%dx%d, strength %.2f, origin (%.1f, %.1f), yaw %.1f deg", levelDir.c_str(), name.c_str(),
						dims[0], dims[1], dims[2], scenery.giStrength, scenery.origin.x, scenery.origin.y, scenery.yaw * 57.29578f);
				}
				else LOG_WARNING("[LevelRuntime] %s%s.json: no dims, origin or size", levelDir.c_str(), name.c_str());
			}
			else if (found) LOG_WARNING("[LevelRuntime] %s%s.json is there but %s.bin does not read", levelDir.c_str(), name.c_str(), name.c_str());
		}
		return true;
	}

	void LevelRuntime::fade_occluders(const std::vector<Pakal::Vec2>& focus)
	{
		if (!m_occluders || m_occluders->empty() || !m_rig) return;
		std::vector<Vec3> targets;
		targets.reserve(focus.size() * 2);
		for (const Pakal::Vec2& at : focus)
		{
			for (const Vec3& point : OccluderFade::body_points(at.x, at.y)) targets.push_back(point);
		}
		m_occluders->update(m_rig->eye(), targets, !targets.empty() && !m_rig->panning(), m_frameSeconds);
	}

	bool LevelRuntime::set_level_object(const std::string& key, bool visible, const std::string& file)
	{
		const auto found = m_levelObjects.find(key);
		if (found == m_levelObjects.end() || found->second.scenery >= m_scenery.size()) return false;
		std::unique_ptr<Pakal::MeshComponent3D_Bgfx>& mesh = m_scenery[found->second.scenery];
		if (!file.empty())
		{
			auto swapped = model(m_levelDir + file);
			if (!swapped || !swapped->is_loaded())
			{
				LOG_WARNING("[LevelRuntime] %s: the model %s does not load", key.c_str(), file.c_str());
				return false;
			}
			// where the old one is drawn now (moved or not)
			swapped->set_matrix(found->second.drawn.data());
			if (m_occluders) m_occluders->replace_mesh(mesh.get(), swapped.get());
			mesh = std::move(swapped);
			// the old model's shadow proxy is not the new one's: the new model casts its own shadow
			if (found->second.proxy < m_scenery.size()) m_scenery[found->second.proxy]->set_visible(false);
			found->second.proxy = kNoProxy;
		}
		mesh->set_visible(visible);
		if (found->second.proxy < m_scenery.size()) m_scenery[found->second.proxy]->set_visible(visible);
		return true;
	}

	void LevelRuntime::draw_level_entry(const LevelEntry& entry)
	{
		if (entry.scenery < m_scenery.size()) m_scenery[entry.scenery]->set_matrix(entry.drawn.data());
		if (entry.proxy < m_scenery.size()) m_scenery[entry.proxy]->set_matrix(entry.drawn.data());
	}

	void LevelRuntime::cast_shadow_through(Pakal::MeshComponent3D_Bgfx& model, Pakal::MeshComponent3D_Bgfx& proxy)
	{
		proxy.set_shadow_only(true);
		model.set_casts_shadow(false);
	}

	bool LevelRuntime::move_level_object(const std::string& key, const Pakal::Vec2& at, float height, float yaw)
	{
		const auto found = m_levelObjects.find(key);
		if (found == m_levelObjects.end() || found->second.scenery >= m_scenery.size()) return false;
		// its placement without its position, turned by 'yaw' about the vertical (rotation_y of the same angle in render
		// space), at the game's point (level x is render x, level y is render -z)
		Mat4 placement = found->second.placement;
		placement[12] = placement[13] = placement[14] = 0.0f;
		const float c = std::cos(yaw), s = std::sin(yaw);
		const Mat4 turn{ c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, at.x, height, -at.y, 1 };
		found->second.drawn = mat4_mul(turn, placement);
		draw_level_entry(found->second);
		return true;
	}

	bool LevelRuntime::reset_level_object(const std::string& key)
	{
		const auto found = m_levelObjects.find(key);
		if (found == m_levelObjects.end() || found->second.scenery >= m_scenery.size()) return false;
		found->second.drawn = found->second.placement;
		draw_level_entry(found->second);
		return true;
	}

	bool LevelRuntime::add_level_object(const std::string& key, const std::string& file, const Pakal::Vec2& at, float height, float yaw,
		float scale)
	{
		if (key.empty() || m_levelObjects.count(key)) return false;
		auto mesh = model(m_levelDir + file);
		if (!mesh || !mesh->is_loaded())
		{
			LOG_WARNING("[LevelRuntime] %s: the model %s does not load", key.c_str(), file.c_str());
			return false;
		}
		// turned by 'yaw' about the vertical, at the game's point (level x is render x, level y is render -z)
		const float c = std::cos(yaw) * scale, s = std::sin(yaw) * scale;
		const Mat4 placement{ c, 0, -s, 0, 0, scale, 0, 0, s, 0, c, 0, at.x, height, -at.y, 1 };
		mesh->set_matrix(placement.data());
		m_levelObjects[key] = LevelEntry{ m_scenery.size(), placement, placement };
		m_scenery.push_back(std::move(mesh));
		return true;
	}

	CharacterViewComponent* LevelRuntime::add_view(Actor& actor, const nlohmann::json& lookData, const CharacterViewComponent::Look& defaults,
		const std::function<void(const std::string& mark)>& onMark, float clay, float dentsPerMeter, size_t variant)
	{
		const CharacterViewComponent::Look look = CharacterViewComponent::Look::from_json(lookData, defaults);
		auto mesh = character_model(look, variant);
		if (!mesh) return nullptr;
		const nlohmann::json marks = lookData.value("marks", nlohmann::json::object());
		for (const auto& [clip, named] : marks.items())
		{
			const int index = mesh->find_animation(clip);
			for (const auto& [name, seconds] : named.items()) mesh->add_clip_mark(index, seconds.get<double>(), name);
		}
		if (onMark) mesh->onClipMark = [onMark](int, const std::string& name) { onMark(name); };
		mesh->set_clay(clay, dentsPerMeter);
		CharacterViewComponent& view = actor.add<CharacterViewComponent>(std::move(mesh), look);
		// what it holds in its hand, when its look says
		if (look.hasHeld && !look.held.mesh.empty()) view.set_held(model(look.held.mesh));
		return &view;
	}

	int LevelRuntime::advance()
	{
		m_frameSeconds = m_clock.frame_seconds();
		return m_clock.advance(m_frameSeconds * m_settings.timeScale);
	}

	void LevelRuntime::sync_views(TopDownGame& game)
	{
		PAKAL_PROFILE_SCOPE("level.views");
		for (const auto& actor : game.actors())
		{
			if (CharacterViewComponent* view = actor->get<CharacterViewComponent>()) view->sync(m_frameSeconds, m_clock.alpha());
		}
	}

	void LevelRuntime::follow(const Pakal::Vec2& focus, double timeMs, float height, float scale)
	{
		if (!m_camera) return;
		const tmath::vector3df p = floor_point(focus, height);
		m_rig->update(Vec3{ p.x, p.y, p.z }, timeMs, scale);
		m_rig->apply(*m_camera, static_cast<int>(m_graphics->get_width()), static_cast<int>(m_graphics->get_height()));
	}

	PlayerInput LevelRuntime::read_input()
	{
		return m_input->read(*m_rig, static_cast<float>(m_graphics->get_width()), static_cast<float>(m_graphics->get_height()));
	}

	void LevelRuntime::end(const std::function<void()>& beforeRelease)
	{
		if (m_ended) return;
		m_ended = true;
		if (m_input) m_input->stop();
		if (!m_graphics) return;
		on_graphics([this, &beforeRelease]()
		{
			m_graphics->clear_active_camera(m_camera.get());
			m_graphics->set_post_process(Pakal::BgfxGraphicsSystem::PostSettings());
			m_graphics->set_shadows(Pakal::BgfxGraphicsSystem::ShadowSettings());
			if (beforeRelease) beforeRelease();   // the game's actors and their models, before the prototypes they share
			if (m_occluders) m_occluders->clear();
			if (m_particles) m_particles->clear();
			if (m_lightmapSet) m_graphics->clear_lightmap();
			m_lightmapSet = false;
			if (m_giSet) m_graphics->clear_gi_volume();
			m_giSet = false;
			if (m_floorHeight) CharacterViewComponent::set_floor(nullptr);
			m_floorHeight = nullptr;
			m_levelObjects.clear();
			if (m_grassField) m_graphics->destroy_grass(m_grassField);
			m_grassField = 0;
			m_scenery.clear();
			m_prototypes.clear();
			m_characters.clear();
			m_camera.reset();
		});
	}
}
