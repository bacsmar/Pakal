///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: EffectsSystem.h
// Purpose: The look of shooting: the bullet streak (the game's bullet texture), the muzzle flash and the
//          shell casing (the game's own models), and the sparks or blood where a bullet lands. The game
//          reports shots and impacts; this draws them. Models are made on the graphics thread.
//          Also the particles of explosions and of props that break: debris, dust, glass, a hydrant's water,
//          a burning car's fire and smoke (BgfxGraphicsSystem::draw_particle).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "RandomStreams.h"
#include "topdown/TopDownGame.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Pakal
{
	class BgfxGraphicsSystem;
	class MeshComponent3D_Bgfx;
}

namespace Pakal::TopDown
{
	class EffectsSystem
	{
	public:
		// Makes a (shared) instance of a model file, or null when it cannot
		using MeshFactory = std::function<std::unique_ptr<Pakal::MeshComponent3D_Bgfx>(const std::string& file)>;

		// A missing file just leaves that effect out. Call on the graphics thread. 'seed': its random stream ("fx").
		EffectsSystem(Pakal::BgfxGraphicsSystem* graphics, const MeshFactory& make, const std::string& bullet,
			const std::string& muzzle, const std::string& shell, uint32_t seed);
		~EffectsSystem();

		void on_shot(const b2Vec2& muzzle, const b2Vec2& direction);
		void on_impact(const b2Vec2& at, const b2Vec2& direction, ImpactKind kind);
		// An explosion of 'radius' meters: a big flash and flying sparks
		void on_blast(const b2Vec2& at, float radius);
		// A ground pound: a ring of dust that runs out to the radius, and a short pale flash on the floor
		void on_shockwave(const b2Vec2& at, float radius);
		// A prop's effect (PropEffectEvent): water from a hydrant, glass, fire and smoke, or "Break" (debris and
		// dust). 'size' is about how wide the prop is (meters).
		void on_prop_effect(const b2Vec2& at, const std::string& effect, float size);

		// Moves and draws everything. Call once per drawn frame.
		void update(float dt, const std::vector<Bullet>& bullets);

		// Drops the models (graphics thread)
		void release();

	private:
		struct Flash
		{
			std::unique_ptr<Pakal::MeshComponent3D_Bgfx> mesh;
			float age = -1.0f;        // < 0: free
			float life = 0.0f;
			float scale = 1.0f;
			float x = 0.0f, y = 0.0f; // level meters
			float height = 1.0f;
			float yaw = 0.0f;
			float color[3] = { 1.0f, 1.0f, 1.0f };
		};

		struct Casing
		{
			std::unique_ptr<Pakal::MeshComponent3D_Bgfx> mesh;
			float age = -1.0f;
			float x = 0.0f, y = 0.0f, h = 0.0f;     // level meters, height
			float vx = 0.0f, vy = 0.0f, vh = 0.0f;
			float spin = 0.0f, turn = 0.0f;
		};

		struct Spark
		{
			float x, y, h;
			float vx, vy, vh;
			float age, life;
			unsigned color;
		};

		// A soft particle: it flies (level meters, height), grows or shrinks from r0 to r1 and fades from a0 to a1 over its life
		struct Puff
		{
			float x, y, h;
			float vx, vy, vh;
			float age, life;
			float r0, r1;
			float a0, a1;
			float red, green, blue;
			float gravity;      // negative rises (fire, smoke)
			bool additive;
			bool bounce;        // a chunk bounces off the floor; the others stop on it
		};

		// Something that keeps giving off particles for a while: a broken hydrant's water, a burnt car's fire
		struct Emitter
		{
			enum class Kind { Water, Fire } kind;
			float x, y, size;
			float secondsLeft;
			float waterDue = 0.0f, fireDue = 0.0f, smokeDue = 0.0f;
		};

		void start_flash(float x, float y, float height, float yaw, float scale, float life, float r, float g, float b);
		void add_puff(const Puff& puff);
		void burst_debris(float x, float y, float size, int count);
		void update_emitters(float dt);
		float random(float low, float high);

		Pakal::BgfxGraphicsSystem* m_graphics;
		std::vector<std::unique_ptr<Pakal::MeshComponent3D_Bgfx>> m_bullets;
		std::vector<Flash> m_flashes;
		std::vector<Casing> m_casings;
		std::vector<Spark> m_sparks;
		std::vector<Puff> m_puffs;
		std::vector<Emitter> m_emitters;
		Pakal::RandomStream m_random;
	};
}
