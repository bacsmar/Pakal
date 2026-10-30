///////////////////////////////////////////////////////////////////////////////
// File: GpuParticles.h
// Description: Particles that live on the GPU: a ring of them in a buffer a compute shader moves every frame
//              (cs_particles) and two instanced draws read (vs_gpu_particle: the blended ones, then the additive ones).
//              The CPU only uploads the new ones. It needs compute and instancing (GLES 3.1, D3D11); without them,
//              available() is false and the game keeps its CPU particles.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <bgfx/bgfx.h>

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

namespace Pakal
{
	class GpuParticles
	{
	public:
		// One particle, in render space (y up): as EffectsSystem's puffs, flying with gravity (negative rises) and landing
		// on the floor (a chunk bounces, the rest stay), its size and alpha going from start to end over its life
		struct Particle
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

		// 'simulate' loads the compute program; 'draw' vs_gpu_particle + fs_particle. Either invalid, or no compute or
		// instancing: not available. The loaders run after the uniforms are made: on OpenGL a program finds its uniforms
		// when it is linked, and one made before them never gets their values (the compute shader saw a capacity of 0
		// and moved nothing: the particles stayed where they were born).
		void create(const std::function<bgfx::ProgramHandle()>& simulate, const std::function<bgfx::ProgramHandle()>& draw,
			uint32_t capacity = 8192);
		void destroy();
		bool available() const { return m_available; }

		// From any thread: a new particle, drawn from the next frame on. Past the capacity the oldest ones go
		void emit(const Particle& particle);

		// From the game's thread: the game's seconds that went by (its own clock: none while it is paused). Once a game
		// calls it, the particles move by those seconds instead of the frame's, so they stop with the game.
		void advance(float seconds);
		// From any thread: every particle gone (a level that ends leaves none behind, still or not)
		void clear();

		// The graphics thread, once a frame: uploads the new ones, moves them all 'dt' seconds (or the game's, see
		// advance) in 'simulateView' (a view that runs before 'drawView'), and draws them in 'drawView'
		void frame(float dt, bgfx::ViewId simulateView, bgfx::ViewId drawView);

	private:
		static const uint32_t kVec4s = 5;   // per particle: position+age, velocity+life, color+gravity, radii+alphas, flags
		bool m_available = false;
		uint32_t m_capacity = 0;
		uint32_t m_head = 0;                // where the next new one goes in the ring
		bgfx::ProgramHandle m_simulate = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle m_draw = BGFX_INVALID_HANDLE;
		bgfx::DynamicVertexBufferHandle m_buffer = BGFX_INVALID_HANDLE;
		bgfx::VertexBufferHandle m_quad = BGFX_INVALID_HANDLE;
		bgfx::IndexBufferHandle m_quadIndices = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_simUniform = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_passUniform = BGFX_INVALID_HANDLE;
		std::mutex m_pendingMutex;
		std::vector<float> m_pending;       // the new ones since the last frame, kVec4s * 4 floats each
		bool m_driven = false;              // the game gives the seconds (advance)
		float m_gameSeconds = 0.0f;         // ... gone by since the last frame
		bool m_clear = false;               // empty the ring at the next frame
	};
}
