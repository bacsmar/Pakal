///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelRuntime.cpp
// Purpose: What every TopDown level state does around its game - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/view/LevelRuntime.h"
#include "topdown/CameraInput.h"
#include "topdown/TopDownGame.h"
#include "topdown/view/RenderSpace.h"

#include "Engine.h"
#include "LogMgr.h"
#include "OSManager.h"
#include "Profiler.h"
#include "bgfx/BgfxGraphicsSystem.h"
#include "bgfx/CameraComponent3D_Bgfx.h"
#include "bgfx/MeshComponent3D_Bgfx.h"

#include <algorithm>
#include <chrono>
#include <thread>

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

	void LevelRuntime::apply_look(const SceneLook& look)
	{
		look.apply(*m_graphics);
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
			const tmath::vector3df p = floor_point(b2Vec2(at[0].get<float>(), at[1].get<float>()));
			const float cx = 0.5f * (low[0] + high[0]), cz = 0.5f * (low[2] + high[2]);
			const float c = std::cos(yaw), s = std::sin(yaw);
			mesh->set_rotation(0.0f, yaw, 0.0f);
			mesh->set_position(p.x - (c * cx + s * cz), p.y - low[1], p.z - (-s * cx + c * cz));
			m_scenery.push_back(std::move(mesh));
		}
	}

	CharacterViewComponent* LevelRuntime::add_view(Actor& actor, const nlohmann::json& lookData, const CharacterViewComponent::Look& defaults,
		const std::function<void(const std::string& mark)>& onMark, float clay, float dentsPerMeter)
	{
		const CharacterViewComponent::Look look = CharacterViewComponent::Look::from_json(lookData, defaults);
		auto mesh = model(look.model);
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

	void LevelRuntime::follow(const b2Vec2& focus, double timeMs, float height, float scale)
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
			m_scenery.clear();
			m_prototypes.clear();
			m_camera.reset();
		});
	}
}
