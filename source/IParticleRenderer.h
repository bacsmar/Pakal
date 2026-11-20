///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: IParticleRenderer.h
// Purpose: What a graphics backend draws for particle systems, with no backend type in sight: soft dots
//          or sprites of a sheet facing the camera, the same lying on the floor (stains), and, where the
//          device can (compute), particles the GPU moves itself. Positions are in render space: x, y up,
//          z (a TopDown level's y is -z). Colors are ABGR. GraphicsSystem::particle_renderer gives it.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <cstdint>
#include <string>

namespace Pakal
{
	class IParticleRenderer
	{
	public:
		// One particle handed to the GPU once: it flies with gravity (meters/s² down; negative rises), lands on a flat floor
		// at 0 (a bouncing one bounces, the rest stay), and goes from radius0/alpha0 to radius1/alpha1 over its life
		struct GpuParticle
		{
			float position[3];
			float velocity[3];
			float life;
			float color[3];
			float gravity;
			float radius0, radius1;
			float alpha0, alpha1;
			bool additive;
			bool bounce;
		};

		virtual ~IParticleRenderer() = default;

		// From the game thread, for one frame (queued for the next one): a particle facing the camera, 'radius' meters, showing
		// the sheet's cell 'sprite' turned 'angle' radians (0: the soft dot). Additive ones add light; the others blend.
		virtual void draw_particle(const float at[3], float radius, uint32_t color, bool additive, uint16_t sprite, float angle) = 0;
		// The same lying flat on the floor (a stain, a pool of light)
		virtual void draw_floor_spot(const float at[3], float radius, uint32_t color, bool additive, uint16_t sprite, float angle) = 0;
		// Graphics thread: the sprite sheet the particles show (a texture cut into columns x rows cells, counted from 1 left to
		// right, top to bottom); an empty path drops it
		virtual void set_sheet(const std::string& texture, int columns, int rows) = 0;
		virtual bool has_sheet() const = 0;

		// Whether the GPU moves particles itself on this device
		virtual bool gpu_available() const = 0;
		// From any thread: a particle the GPU takes from the next frame on
		virtual void emit_gpu(const GpuParticle& particle) = 0;
		// From the game thread: the game's seconds (none while paused) move the GPU's particles
		virtual void advance_gpu(float seconds) = 0;
		// From any thread: none of the GPU's particles left
		virtual void clear_gpu() = 0;
	};
}
