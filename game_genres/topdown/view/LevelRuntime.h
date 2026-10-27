///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelRuntime.h
// Purpose: What every TopDown level state does around its game: the camera (a CameraRig and the bgfx
//          camera), the player's input (InputMapper), the models (instances of prototypes in an
//          AssetCache, decoded in the background and shared), the scenery and the scene's look, each
//          character's animated model, the fixed steps of the simulation (SimulationClock) and the
//          models drawn between them, and letting it all go on the graphics thread at the end. A game's
//          state owns one and keeps only what is its own (its rules, its game, its interface).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "AssetCache.h"
#include "RandomStreams.h"
#include "SimulationClock.h"
#include "topdown/view/CameraRig.h"
#include "topdown/view/CharacterViewComponent.h"
#include "topdown/view/InputMapper.h"
#include "topdown/view/SceneLook.h"

#include <nlohmann/json.hpp>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Pakal
{
	class BgfxGraphicsSystem;
	class CameraComponent3D_Bgfx;
	class Engine;
	class MeshComponent3D_Bgfx;
}

namespace Pakal::TopDown
{
	class TopDownGame;

	class LevelRuntime
	{
	public:
		struct Settings
		{
			CameraRig::Settings camera;
			InputMapper::Settings input;
			float nearPlane = 0.3f, farPlane = 500.0f;
			float timeScale = 1.0f;   // game seconds per real second (a bot's runs)
			uint32_t seed = 2026;     // the presentation's random streams ("shake")
		};

		explicit LevelRuntime(Pakal::Engine& engine);
		~LevelRuntime();
		LevelRuntime(const LevelRuntime&) = delete;
		LevelRuntime& operator=(const LevelRuntime&) = delete;

		// The camera and the input; false without a graphics system
		bool begin(const Settings& settings);
		// Runs on the graphics thread and waits for it (models are made there)
		void on_graphics(const std::function<void()>& work);

		// Graphics thread: an instance of a model file (its prototype is loaded once, in the background when asked for
		// early with prefetch, and shared); nullptr when the file does not load
		std::unique_ptr<Pakal::MeshComponent3D_Bgfx> model(const std::string& file);
		// Any thread: start decoding these files now
		void prefetch(const std::vector<std::string>& files);
		// Graphics thread: the scene's light and picture
		void apply_look(const SceneLook& look);
		// Graphics thread: models standing on the floor, each centered on its point: [{ "model", "at": [x, y], "yaw" }]
		void place_scenery(const nlohmann::json& items, const std::string& folder = std::string());
		// Graphics thread: a character's animated model from its look (a JSON look over 'defaults'; its "marks" on its clips
		// call 'onMark' with the mark's name; 'clay' makes it plasticine). Its model file is the look's "model", and what it
		// holds the look's "held" mesh.
		CharacterViewComponent* add_view(Actor& actor, const nlohmann::json& look, const CharacterViewComponent::Look& defaults,
			const std::function<void(const std::string& mark)>& onMark = {}, float clay = 0.0f, float dentsPerMeter = 9.0f);

		// Each frame: how many fixed steps the simulation runs now (call the game's step that many times with step_seconds())
		int advance();
		float step_seconds() const { return m_clock.step(); }
		float alpha() const { return m_clock.alpha(); }
		float frame_seconds() const { return m_frameSeconds; }
		// Each frame, after the steps: the characters' models, between the last step and the next
		void sync_views(TopDownGame& game);
		// Each frame: the camera follows this point of the floor (meters), 'height' over it
		void follow(const b2Vec2& focus, double timeMs, float height = 0.9f, float scale = 1.0f);
		// This step's input of the player, from the keys, the mouse and the sticks
		PlayerInput read_input();

		Pakal::BgfxGraphicsSystem& graphics() { return *m_graphics; }
		CameraRig& rig() { return *m_rig; }
		InputMapper& input() { return *m_input; }
		Pakal::AssetCache& assets() { return *m_assets; }
		Pakal::RandomStreams& random() { return m_random; }

		// Lets the models, the camera and the look go on the graphics thread (also when it is destroyed). Characters' models
		// belong to the game's actors: the game is destroyed before this (or in 'beforeRelease').
		void end(const std::function<void()>& beforeRelease = {});

	private:
		Pakal::Engine& m_engine;
		Pakal::BgfxGraphicsSystem* m_graphics = nullptr;
		Settings m_settings;
		Pakal::SimulationClock m_clock;
		Pakal::RandomStreams m_random;
		std::unique_ptr<CameraRig> m_rig;
		std::unique_ptr<InputMapper> m_input;
		std::unique_ptr<Pakal::CameraComponent3D_Bgfx> m_camera;
		std::shared_ptr<Pakal::AssetCache> m_assets;
		std::map<std::string, Pakal::AssetHandle<Pakal::MeshComponent3D_Bgfx>> m_prototypes;
		std::vector<std::unique_ptr<Pakal::MeshComponent3D_Bgfx>> m_scenery;
		float m_frameSeconds = 0.0f;
		bool m_ended = false;
	};
}
