///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ParticleEffects.h
// Purpose: Particle effects made of data, not code: an effect file (JSON, data/vfx/<name>.json) holds one
//          or more emitters (their shape, bursts and rates, ranges of life, speed, size and turn, gravity
//          and drag, color and size over life, a flipbook of the sprite sheet, landing on the floor,
//          stains left on it, effects started where a particle dies or lands). The game plays an effect at
//          a point of the floor and gets a handle to move or stop it. Every emitter runs on the CPU; one
//          that only uses what the GPU's particles do (one color fading, size from start to end, gravity,
//          landing on a flat floor) is handed to the GPU when the device has compute (gpu: "auto").
//          Everything is in the game's frame: x and y on the floor, height up. Game thread.
//          The file format: docs/PARTICLES.md.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "RandomStreams.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "math/Vec2.h"

namespace Pakal
{
	class IParticleRenderer;
}

namespace Pakal::TopDown
{
	class ParticleEffects
	{
	public:
		using Handle = uint32_t;   // 0: none

		// A value over a particle's life (0..1): points [t, value], straight between them
		struct Curve
		{
			std::vector<std::array<float, 2>> points;
			float at(float t) const;
			static Curve read(const nlohmann::json& data, float constant);
		};
		// A color over a particle's life: stops [t, r, g, b, a]
		struct Gradient
		{
			std::vector<std::array<float, 5>> stops;
			std::array<float, 4> at(float t) const;
			static Gradient read(const nlohmann::json& data);
		};
		struct Range
		{
			float low = 0.0f, high = 0.0f;
			static Range read(const nlohmann::json& data, float low, float high);
		};

		struct Effect;
		struct Emitter
		{
			enum class Shape { Point, Sphere, Cone, Box } shape = Shape::Point;
			float radius = 0.0f;                     // sphere and cone: where particles start (meters around the point)
			float angle = 0.4363f;                   // cone: half its opening (radians, from "angle" in degrees)
			float elevation = 0.0f;                  // cone: its axis raised over the play direction (radians, from degrees)
			std::array<float, 3> box{ 0.0f, 0.0f, 0.0f };   // box: its half sizes (x along the direction, y across, height)
			int burst = 0;                           // particles at its start
			float rate = 0.0f;                       // particles a second while it runs
			float duration = 0.0f, delay = 0.0f;
			bool loop = false;
			int most = 256;                          // live particles of this emitter in one effect at most
			Range life{ 0.5f, 1.0f }, speed{ 1.0f, 2.0f }, size{ 0.1f, 0.2f }, rotation{ 0.0f, 0.0f }, spin{ 0.0f, 0.0f };
			float startHeight = 0.0f;                // over the effect's height
			float gravity = 0.0f;                    // meters/s² down (negative rises)
			float drag = 0.0f;                       // share of the speed lost each second
			Curve sizeOverLife;                      // times 'size'
			Gradient color;
			bool additive = false;
			int sprite = 0, frames = 1;              // the sheet's cell (1..; 0: the soft dot) and how many cells it plays
			float fps = 0.0f;                        // 0: the frames spread over its life
			enum class Floor { None, Stay, Bounce } floor = Floor::None;
			float restitution = 0.35f, friction = 0.7f;
			struct Decal
			{
				bool on = false;
				int sprite = 0;
				Range size{ 0.2f, 0.4f };
				float seconds = 6.0f, fade = 1.5f;
				std::array<float, 4> color{ 1.0f, 1.0f, 1.0f, 1.0f };
				bool additive = false;
			} decal;
			std::shared_ptr<const Effect> onDeath, onLand;   // played where a particle dies or first lands
			enum class Gpu { Auto, Never } gpu = Gpu::Auto;
			bool fitsGpu = false;                    // what it uses is what the GPU's particles do
		};
		struct Effect
		{
			std::vector<Emitter> emitters;
			float length = 0.0f;                     // seconds until no emitter starts particles any more (looping: none)
		};

		// 'renderer': GraphicsSystem::particle_renderer (null: nothing is drawn, the particles still move)
		ParticleEffects(Pakal::IParticleRenderer* renderer, uint32_t seed);
		~ParticleEffects();

		// Any thread: read these files now (they are read once and kept)
		void prefetch(const std::vector<std::string>& files);
		// Graphics thread: the sprite sheet of the effects (data/vfx/sheet.json: { "texture", "columns", "rows",
		// "sprites": { name: cell } }); emitters name their sprite by it. False when it does not read.
		bool load_sheet(const std::string& file);
		// The floor's height at a point (LevelRuntime::floor_height); empty: flat at 0
		void set_floor(std::function<float(const Pakal::Vec2&)> floor) { m_floor = std::move(floor); }

		// An effect at a point of the floor, 'height' over it, facing 'direction' (the cone's axis, the box's x), 'scale'
		// times its sizes, speeds and shapes. 0 when the file does not read.
		Handle play(const std::string& file, const Pakal::Vec2& at, float height = 0.0f, const Pakal::Vec2& direction = Pakal::Vec2(1.0f, 0.0f),
			float scale = 1.0f);
		// No more particles from it; 'clear': its live ones go too
		void stop(Handle handle, bool clear = false);
		// Where it emits from now (a fire on a moving car)
		void set_position(Handle handle, const Pakal::Vec2& at, float height, const Pakal::Vec2& direction);
		bool playing(Handle handle) const;
		// Everything gone (a level that ends)
		void clear();

		// Each frame, with the game's seconds (none while paused): moves the particles and draws them
		void update(float seconds);

		size_t live() const { return m_particles.size(); }

	private:
		struct Particle
		{
			float x, y, h, vx, vy, vh;
			float age, life, size, rotation, spin;
			const Emitter* emitter;
			std::shared_ptr<const Effect> effect;   // keeps its emitter alive
			Handle owner;
			bool landed;
		};
		struct Running
		{
			Handle handle;
			std::shared_ptr<const Effect> effect;
			float x, y, h, dx, dy, scale;
			float time = 0.0f;
			std::vector<float> owed;                 // each emitter's particles due but not made yet (a rate's fractions)
			std::vector<int> alive;                  // each emitter's live particles
			std::vector<bool> burst;
			bool stopped = false;
		};
		struct Decal
		{
			float x, y, h, size, rotation, age;
			const Emitter::Decal* decal;
			std::shared_ptr<const Effect> effect;
		};

		std::shared_ptr<const Effect> effect(const std::string& file);
		std::shared_ptr<const Effect> parse(const nlohmann::json& data, const std::string& file, int depth);
		void spawn(Running& running, size_t index, int count);
		void play_at(const std::shared_ptr<const Effect>& effect, float x, float y, float h, float dx, float dy, float scale);
		float floor_at(float x, float y) const { return m_floor ? m_floor(Pakal::Vec2(x, y)) : 0.0f; }
		void draw(const Particle& particle) const;

		Pakal::IParticleRenderer* m_graphics;
		Pakal::RandomStream m_random;
		std::function<float(const Pakal::Vec2&)> m_floor;
		std::map<std::string, int> m_sprites;
		mutable std::mutex m_filesMutex;
		std::map<std::string, std::shared_ptr<const Effect>> m_files;
		std::vector<Running> m_running;
		std::vector<Particle> m_particles;
		std::vector<Decal> m_decals;
		Handle m_next = 1;
		static const size_t kMostParticles = 4096, kMostDecals = 256;
	};
}
