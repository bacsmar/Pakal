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

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "math/Vec2.h"

namespace Pakal
{
	class BgfxGraphicsSystem;
	class CameraComponent3D_Bgfx;
	class Engine;
	class MeshComponent3D_Bgfx;
	struct LevelObject;
}

namespace Pakal::TopDown
{
	class OccluderFade;
	class ParticleEffects;
	class TopDownGame;

	// How a level document's scenery is drawn (place_level)
	struct LevelScenery
	{
		bool lightmap = true;              // the level's baked light (doc.extra "lightmap"), when it has one
		// A building that hides the player: "cut" (solid, cut away above 'keep' meters over its base, a rim of 'rim'
		// meters in 'rimColor' at the cut; 'floor' is its opacity while cut), "blend" (see-through) or "dither". Trees
		// always dither.
		std::string buildingFade = "cut";
		float keep = 1.2f, rim = 0.08f, floor = 0.3f;
		float rimColor[3] = { 1.0f, 0.86f, 0.5f };
		// The level's point (meters, level plane) drawn at the world's origin: a game whose simulation has its own frame (a
		// brawler's street from 0) sees the level moved by -origin. The floor heights (floor_height, the characters'
		// set_floor) and the occluders are moved the same way.
		Pakal::Vec2 origin = Pakal::Vec2(0.0f, 0.0f);
		// The level turned about 'origin' by this many radians (counterclockwise seen from above) after the move: a street
		// that runs at 'yaw' in the level runs along +x in the world. A world point p is the level's origin + R(yaw) p; the
		// floor heights and the occluders turn with it. The sun (SceneLook) does not.
		float yaw = 0.0f;
		// The level's baked ambient light (light probes: gi.json and gi.bin next to the level, from the level builder), for
		// the characters and the models without a lightmap; 'giLook' takes gi_<giLook>.json instead when there is one (a time
		// of day). 'giStrength': how much of the ambient light comes from it.
		bool gi = true;
		std::string giLook;
		float giStrength = 0.75f;
	};

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
		// Graphics thread: the same without waiting: nullptr while the file is still decoding in the background (ask again
		// next frame), and 'failed' set when it does not load
		std::unique_ptr<Pakal::MeshComponent3D_Bgfx> model_if_ready(const std::string& file, bool& failed);
		// Graphics thread: the scene's light and picture
		void apply_look(const SceneLook& look);
		// Any thread: the wind in the foliage (every alpha-tested model: leaves, palms, grass bend with gusts), 'bend' meters
		// at 1 m above a model's origin, at 'speed'; 0 turns it off (e.g. 0.035, 1.2). For the games that may not see
		// the backend (ADR-0017)
		static void set_wind(float bend, float speed);
		// Any thread: how far models are drawn: up to their size times 'perMeter' meters away, and always within 'minimum'
		// (0: everything). A low-end phone draws a scene's small plants nearer only
		static void set_draw_distance(float perMeter, float minimum);
		// Graphics thread: models standing on the floor, each centered on its point: [{ "model", "at": [x, y], "yaw" }]
		void place_scenery(const nlohmann::json& items, const std::string& folder = std::string());
		// Graphics thread: the scenery of a level document (a level.json from the level builder or the editor, a mission's
		// layer composed with its base): each object's model placed by its matrix, with its building paint or tint and its
		// rect of the level's lightmap; the lightmap itself; the floor's heights for the characters
		// (CharacterViewComponent::set_floor, from the document's floor grid when it has one, see floor_height); and the
		// buildings and trees that may hide the player, for fade_occluders. 'keep' filters the objects (empty: all).
		// Objects made from a prefab (no mesh of their own) are not drawn yet. The models are the runtime's own, let go at
		// end(), and the lightmap and the floor are cleared there. False when the file does not read.
		bool place_level(const std::string& levelPath, const std::function<bool(const Pakal::LevelObject&)>& keep = {},
			const LevelScenery& scenery = LevelScenery());
		// The floor's height (meters) at a point of the level, from the last place_level's floor grid; empty without one
		const std::function<float(const Pakal::Vec2&)>& floor_height() const { return m_floorHeight; }
		// Each frame, after follow: the buildings and trees between the camera and these points of the floor (characters'
		// chests and heads over them) fade, and the rest come back. Empty, or while the camera pans: all come back.
		void fade_occluders(const std::vector<Pakal::Vec2>& focus);
		// Graphics thread: an object of the last place_level, by its key (LevelObject::key): hidden or shown, and its model
		// swapped when 'model' is given (a broken version: a path the level's folder starts, placed where the object was;
		// it keeps no lightmap rect or paint). False when no object has that key or the model does not load.
		bool set_level_object(const std::string& key, bool visible, const std::string& model = std::string());
		// Graphics thread: where an object of the last place_level is drawn now (a thrown prop), in the game's frame: its
		// point of the floor, its height over the floor's zero and its turn (radians, counterclockwise seen from above) over
		// the one it was placed with; its scale and its own tilt stay. False when no object has that key.
		bool move_level_object(const std::string& key, const Pakal::Vec2& at, float height, float yaw = 0.0f);
		// Graphics thread: the object back where place_level put it
		bool reset_level_object(const std::string& key);
		// Graphics thread: a new object among the last place_level's (a pickup dropped, a thing thrown in): its model (a path
		// the level's folder starts) at the game's point, height and turn (as move_level_object), 'scale' times its size; then
		// set_level_object, move_level_object and reset_level_object take it by its key. False when the key is taken or the
		// model does not load
		bool add_level_object(const std::string& key, const std::string& model, const Pakal::Vec2& at, float height = 0.0f,
			float yaw = 0.0f, float scale = 1.0f);
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
		void follow(const Pakal::Vec2& focus, double timeMs, float height = 0.9f, float scale = 1.0f);
		// This step's input of the player, from the keys, the mouse and the sticks
		PlayerInput read_input();

		Pakal::BgfxGraphicsSystem& graphics() { return *m_graphics; }
		CameraRig& rig() { return *m_rig; }
		// The bgfx camera the rig drives (a tool places it itself: an editor's free camera)
		Pakal::CameraComponent3D_Bgfx& camera() { return *m_camera; }
		InputMapper& input() { return *m_input; }
		Pakal::AssetCache& assets() { return *m_assets; }
		Pakal::RandomStreams& random() { return m_random; }
		// The particle effects of this level (data/vfx files: ParticleEffects), standing on place_level's floor. Made by
		// begin; the game calls its update each frame with its own seconds, and end() clears it.
		ParticleEffects& particles() { return *m_particles; }

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
		std::unique_ptr<OccluderFade> m_occluders;
		std::unique_ptr<ParticleEffects> m_particles;
		std::function<float(const Pakal::Vec2&)> m_floorHeight;
		bool m_lightmapSet = false;
		bool m_giSet = false;
		// The keyed objects of the last place_level: their model in m_scenery and their matrices (render space)
		struct LevelEntry { size_t scenery = 0; std::array<float, 16> placement{}, drawn{}; };   // as placed, and as drawn now
		std::map<std::string, LevelEntry> m_levelObjects;
		std::string m_levelDir;
		float m_frameSeconds = 0.0f;
		bool m_ended = false;
	};
}
