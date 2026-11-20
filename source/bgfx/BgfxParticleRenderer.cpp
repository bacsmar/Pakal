///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: BgfxParticleRenderer.cpp
// Purpose: IParticleRenderer on bgfx - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "bgfx/BgfxParticleRenderer.h"
#include "bgfx/BgfxGraphicsSystem.h"

namespace Pakal
{
	void BgfxParticleRenderer::draw_particle(const float at[3], float radius, uint32_t color, bool additive, uint16_t sprite, float angle)
	{
		const tmath::vector3df p(at[0], at[1], at[2]);
		if (sprite > 0 && m_graphics.has_particle_sprites()) m_graphics.draw_sprite_particle(p, radius, color, additive, sprite, angle);
		else m_graphics.draw_particle(p, radius, color, additive);
	}

	void BgfxParticleRenderer::draw_floor_spot(const float at[3], float radius, uint32_t color, bool additive, uint16_t sprite, float angle)
	{
		const tmath::vector3df p(at[0], at[1], at[2]);
		if (sprite > 0 && m_graphics.has_particle_sprites()) m_graphics.draw_sprite_spot(p, radius, color, additive, sprite, angle);
		else m_graphics.draw_ground_spot(p, radius, color, additive);
	}

	void BgfxParticleRenderer::set_sheet(const std::string& texture, int columns, int rows)
	{
		m_graphics.set_particle_sprites(texture, columns, rows);
	}

	bool BgfxParticleRenderer::has_sheet() const
	{
		return m_graphics.has_particle_sprites();
	}

	bool BgfxParticleRenderer::gpu_available() const
	{
		return m_graphics.gpu_particles();
	}

	void BgfxParticleRenderer::emit_gpu(const GpuParticle& particle)
	{
		GpuParticles::Particle p;
		for (int i = 0; i < 3; ++i)
		{
			p.position[i] = particle.position[i];
			p.velocity[i] = particle.velocity[i];
			p.color[i] = particle.color[i];
		}
		p.life = particle.life;
		p.gravity = particle.gravity;
		p.radius0 = particle.radius0; p.radius1 = particle.radius1;
		p.alpha0 = particle.alpha0; p.alpha1 = particle.alpha1;
		p.additive = particle.additive;
		p.bounce = particle.bounce;
		m_graphics.emit_gpu_particle(p);
	}

	void BgfxParticleRenderer::advance_gpu(float seconds)
	{
		m_graphics.advance_gpu_particles(seconds);
	}

	void BgfxParticleRenderer::clear_gpu()
	{
		m_graphics.clear_gpu_particles();
	}
}
