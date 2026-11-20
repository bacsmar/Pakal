///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: BgfxParticleRenderer.h
// Purpose: IParticleRenderer on bgfx: BgfxGraphicsSystem's particles, floor spots, sprite sheet and
//          GPU particles (GpuParticles)
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "IParticleRenderer.h"

namespace Pakal
{
	class BgfxGraphicsSystem;

	class BgfxParticleRenderer final : public IParticleRenderer
	{
	public:
		explicit BgfxParticleRenderer(BgfxGraphicsSystem& graphics) : m_graphics(graphics) {}

		void draw_particle(const float at[3], float radius, uint32_t color, bool additive, uint16_t sprite, float angle) override;
		void draw_floor_spot(const float at[3], float radius, uint32_t color, bool additive, uint16_t sprite, float angle) override;
		void set_sheet(const std::string& texture, int columns, int rows) override;
		bool has_sheet() const override;
		bool gpu_available() const override;
		void emit_gpu(const GpuParticle& particle) override;
		void advance_gpu(float seconds) override;
		void clear_gpu() override;

	private:
		BgfxGraphicsSystem& m_graphics;
	};
}
