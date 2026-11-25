///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: BgfxGrass.h
// Purpose: Grass made on the GPU (GraphicsSystem::create_grass, docs/GRASS.md). Each field is cut in square tiles; one
//          tile mesh holds its blades in a random order (their rank), so drawing the first k of them is an even sample
//          of k. A frame sorts the tiles near the focus into four rings by distance and draws each ring in one
//          instanced draw with as many blades as its density needs; the vertex shader (vs_grass) shapes each blade from
//          its tile's place, the field's maps and the wind, and folds away the blades the density leaves out. Solid,
//          double-sided, casts no shadow. Graphics thread.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "GraphicsSystem.h"

#include <bgfx/bgfx.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>

namespace Pakal
{
	class BgfxGrass
	{
	public:
		BgfxGrass() = default;
		~BgfxGrass();
		BgfxGrass(const BgfxGrass&) = delete;
		BgfxGrass& operator=(const BgfxGrass&) = delete;

		// The uniforms, then the program from 'load' (vs_grass + fs_grass: on OpenGL a program finds its uniforms when it is
		// linked); without the program nothing is drawn
		void create(const std::function<bgfx::ProgramHandle()>& load);
		// Every field and GPU resource (before bgfx goes)
		void destroy();
		bool available() const { return bgfx::isValid(m_program); }

		// A field from its description (GraphicsSystem::GrassField); false (logged) when it cannot be made
		bool add(GraphicsSystem::GrassHandle handle, const GraphicsSystem::GrassField& field);
		void remove(GraphicsSystem::GrassHandle handle);
		void set_scale(float reach, float density) { m_reachScale = reach; m_densityScale = density; }
		void set_shadows(bool on) { m_shadows = on; }
		size_t count() const { return m_fields.size(); }

		// Draws the fields in the main view: 'eye' for the blades' facing, the cover's focus and the frustum from
		// MeshComponent3D_Bgfx (shared with the scattered models). Returns the blades drawn (an upper bound: those the
		// density folds away included).
		uint32_t render(bgfx::ViewId viewId, const float eye[3], double seconds);

	private:
		struct Field
		{
			GraphicsSystem::GrassField desc;
			float tile = 4.0f;              // a tile's side (m)
			uint32_t blades = 0;            // blades in the tile mesh
			float heightLow = 0.0f, heightHigh = 0.0f;   // the ground's lowest and highest point
			bgfx::VertexBufferHandle vertices = BGFX_INVALID_HANDLE;
			bgfx::IndexBufferHandle indices = BGFX_INVALID_HANDLE;
			bgfx::IndexBufferHandle indicesFar = BGFX_INVALID_HANDLE;   // one triangle a blade, for the far rings
			bgfx::TextureHandle cover = BGFX_INVALID_HANDLE;    // R density, G 0, B 0, A 255
			bgfx::TextureHandle ground = BGFX_INVALID_HANDLE;   // the ground's height in 16 bits (R high, G low)
		};
		void free_field(Field& field);

		bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_uField = BGFX_INVALID_HANDLE;   // u_grassField: x, y the rectangle's corner, z, w 1 / its size
		bgfx::UniformHandle m_uShape = BGFX_INVALID_HANDLE;   // u_grassShape: heights, width, the ground's height
		bgfx::UniformHandle m_uReach = BGFX_INVALID_HANDLE;   // u_grassReach: reach, density, the ground's range, tile
		bgfx::UniformHandle m_uGround = BGFX_INVALID_HANDLE;  // u_grassGround: the height map's size, lowest point, range
		bgfx::UniformHandle m_uFocus = BGFX_INVALID_HANDLE;   // u_grassFocus: xyz the focus, w the clock
		bgfx::UniformHandle m_uEye = BGFX_INVALID_HANDLE;     // u_grassEye: xyz the camera
		bgfx::UniformHandle m_uWind = BGFX_INVALID_HANDLE;    // u_grassWind: bend, speed
		bgfx::UniformHandle m_uRoot = BGFX_INVALID_HANDLE;    // u_grassRoot, u_grassTip: the colors
		bgfx::UniformHandle m_uTip = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_uClumpColors = BGFX_INVALID_HANDLE;   // u_grassClumpColors[3]: rgb, w the share of the clumps
		bgfx::UniformHandle m_uDead = BGFX_INVALID_HANDLE;    // u_grassDead: rgb, w the share of dead blades
		bgfx::UniformHandle m_uClump = BGFX_INVALID_HANDLE;   // u_grassClump: 1 / clump size, strength, tuft blades, spread
		bgfx::UniformHandle m_uThin = BGFX_INVALID_HANDLE;    // u_grassThin: x where thinning starts (share of the reach), y the density at the reach
		bgfx::UniformHandle m_uShadowMtx = BGFX_INVALID_HANDLE;   // u_grassShadowMtx: the sun's (MeshComponent3D_Bgfx's)
		bgfx::UniformHandle m_uShadow = BGFX_INVALID_HANDLE;  // u_grassShadow: x strength (0 none), y bias
		bgfx::UniformHandle m_sShadow = BGFX_INVALID_HANDLE;  // s_grassShadow: the sun's shadow map, read at each root
		bgfx::UniformHandle m_sCover = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_sGround = BGFX_INVALID_HANDLE;
		std::unordered_map<GraphicsSystem::GrassHandle, std::unique_ptr<Field>> m_fields;
		float m_reachScale = 1.0f, m_densityScale = 1.0f;
		bool m_shadows = true;
	};
}
