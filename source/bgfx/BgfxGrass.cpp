///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: BgfxGrass.cpp
// Purpose: Grass made on the GPU - implementation (see BgfxGrass.h, docs/GRASS.md)
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "BgfxGrass.h"
#include "MeshComponent3D_Bgfx.h"
#include "LogMgr.h"
#include "Profiler.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Pakal
{
	namespace
	{
		// A blade's five corners: two at the root, two halfway up, the tip; three triangles
		constexpr int kBladeVertices = 5;
		constexpr int kBladeIndices = 9;
		constexpr int kBladeIndicesFar = 3;   // the far rings' blades: one triangle, root to tip (a pixel or two wide)
		constexpr int kFarRing = 2;           // from this ring on
		constexpr int kRings = 4;
		// The rings' outer edges, as shares of the reach: the nearest draws every blade, each farther one fewer
		constexpr float kRingEdge[kRings] = { 0.3f, 0.55f, 0.8f, 1.0f };

		struct GrassVertex
		{
			float x, t, z;        // the root's place in the tile (m), the height along the blade (0 root, 1 tip)
			float side, rank;     // -1 left edge, +1 right, 0 the tip; the blade's rank (0..1: drawn when under the density)
			float r1, r2;         // two random numbers of the blade (its facing; its height and color within its clump)
		};

		bgfx::VertexLayout& grass_layout()
		{
			static bgfx::VertexLayout layout;
			static bool made = false;
			if (!made)
			{
				layout.begin()
					.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
					.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
					.add(bgfx::Attrib::TexCoord1, 2, bgfx::AttribType::Float)
					.end();
				made = true;
			}
			return layout;
		}

		uint32_t hash32(uint32_t x)
		{
			x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
			return x;
		}
		float random01(uint32_t seed) { return (hash32(seed) & 0xFFFFFF) / float(0x1000000); }

		uint32_t reverse_bits(uint32_t v, int bits)
		{
			uint32_t r = 0;
			for (int i = 0; i < bits; ++i) { r = (r << 1) | (v & 1u); v >>= 1; }
			return r;
		}

		// The share of a field's blades kept at 'distance' from the focus: all of them near, fewer out to the reach
		// (vs_grass works out the same, blade by blade)
		float density_at(float distance, float reach, float scale, float thinFrom, float farDensity)
		{
			const float a = thinFrom * reach, b = reach;
			const float x = std::clamp((distance - a) / std::max(0.001f, b - a), 0.0f, 1.0f);
			return std::clamp(scale * (1.0f - (1.0f - farDensity) * x * x * (3.0f - 2.0f * x)), 0.0f, 1.0f);
		}
	}

	BgfxGrass::~BgfxGrass()
	{
		destroy();
	}

	void BgfxGrass::create(const std::function<bgfx::ProgramHandle()>& load)
	{
		m_uField = bgfx::createUniform("u_grassField", bgfx::UniformType::Vec4);
		m_uShape = bgfx::createUniform("u_grassShape", bgfx::UniformType::Vec4);
		m_uReach = bgfx::createUniform("u_grassReach", bgfx::UniformType::Vec4);
		m_uGround = bgfx::createUniform("u_grassGround", bgfx::UniformType::Vec4);
		m_uFocus = bgfx::createUniform("u_grassFocus", bgfx::UniformType::Vec4);
		m_uEye = bgfx::createUniform("u_grassEye", bgfx::UniformType::Vec4);
		m_uWind = bgfx::createUniform("u_grassWind", bgfx::UniformType::Vec4);
		m_uRoot = bgfx::createUniform("u_grassRoot", bgfx::UniformType::Vec4);
		m_uTip = bgfx::createUniform("u_grassTip", bgfx::UniformType::Vec4);
		m_uClumpColors = bgfx::createUniform("u_grassClumpColors", bgfx::UniformType::Vec4, 3);
		m_uDead = bgfx::createUniform("u_grassDead", bgfx::UniformType::Vec4);
		m_uClump = bgfx::createUniform("u_grassClump", bgfx::UniformType::Vec4);
		m_uThin = bgfx::createUniform("u_grassThin", bgfx::UniformType::Vec4);
		m_uShadowMtx = bgfx::createUniform("u_grassShadowMtx", bgfx::UniformType::Mat4);
		m_uShadow = bgfx::createUniform("u_grassShadow", bgfx::UniformType::Vec4);
		m_sShadow = bgfx::createUniform("s_grassShadow", bgfx::UniformType::Sampler);
		m_sCover = bgfx::createUniform("s_grassCover", bgfx::UniformType::Sampler);
		m_sGround = bgfx::createUniform("s_grassGround", bgfx::UniformType::Sampler);
		m_program = load ? load() : bgfx::ProgramHandle(BGFX_INVALID_HANDLE);
		if (!available()) LOG_WARNING("[BgfxGrass] no grass program (vs_grass, fs_grass): no grass is drawn");
	}

	void BgfxGrass::destroy()
	{
		for (auto& [handle, field] : m_fields) free_field(*field);
		m_fields.clear();
		for (bgfx::UniformHandle* u : { &m_uField, &m_uShape, &m_uReach, &m_uGround, &m_uFocus, &m_uEye, &m_uWind, &m_uRoot,
			&m_uTip, &m_uClumpColors, &m_uDead, &m_uClump, &m_uThin, &m_uShadowMtx, &m_uShadow, &m_sShadow, &m_sCover, &m_sGround })
		{
			if (bgfx::isValid(*u)) bgfx::destroy(*u);
			*u = BGFX_INVALID_HANDLE;
		}
		if (bgfx::isValid(m_program)) bgfx::destroy(m_program);
		m_program = BGFX_INVALID_HANDLE;
	}

	void BgfxGrass::free_field(Field& field)
	{
		if (bgfx::isValid(field.vertices)) bgfx::destroy(field.vertices);
		if (bgfx::isValid(field.indices)) bgfx::destroy(field.indices);
		if (bgfx::isValid(field.indicesFar)) bgfx::destroy(field.indicesFar);
		field.indicesFar = BGFX_INVALID_HANDLE;
		if (bgfx::isValid(field.cover)) bgfx::destroy(field.cover);
		if (bgfx::isValid(field.ground)) bgfx::destroy(field.ground);
		field.vertices = BGFX_INVALID_HANDLE;
		field.indices = BGFX_INVALID_HANDLE;
		field.cover = BGFX_INVALID_HANDLE;
		field.ground = BGFX_INVALID_HANDLE;
	}

	bool BgfxGrass::add(GraphicsSystem::GrassHandle handle, const GraphicsSystem::GrassField& desc)
	{
		if (!available()) return false;
		if (!(desc.maxX > desc.minX) || !(desc.maxZ > desc.minZ) || desc.bladesPerSquareMeter <= 0.0f)
		{
			LOG_WARNING("[BgfxGrass] a grass field with no area or no blades: not made");
			return false;
		}
		auto field = std::make_unique<Field>();
		field->desc = desc;
		field->desc.heightMin = std::max(0.01f, desc.heightMin);
		field->desc.heightMax = std::max(field->desc.heightMin, desc.heightMax);
		field->desc.reach = std::max(1.0f, desc.reach);

		// The tile: a grid of g x g blades (g a power of two), as wide as the density wants; a coarser grid when the tiles
		// would be too large (a thin field)
		int bits = 5;   // 32 x 32
		float tile = 32.0f / std::sqrt(desc.bladesPerSquareMeter);
		if (tile > 8.0f) { bits = 4; tile = 16.0f / std::sqrt(desc.bladesPerSquareMeter); }
		tile = std::clamp(tile, 1.0f, 12.0f);
		const int g = 1 << bits;
		const uint32_t blades = uint32_t(g * g);
		field->tile = tile;
		field->blades = blades;

		// The blades in rank order: rank r's cell is r's bits reversed, so every prefix of the list spreads evenly over the
		// tile (the first quarter is every other blade both ways, and so on); a jitter inside the cell breaks the grid
		std::vector<GrassVertex> vertices;
		vertices.reserve(blades * kBladeVertices);
		std::vector<uint16_t> indices;
		indices.reserve(blades * kBladeIndices);
		std::vector<uint16_t> indicesFar;
		indicesFar.reserve(blades * kBladeIndicesFar);
		const float cell = tile / g;
		// tufts of about tuftBlades blades: squares of tuftSide x tuftSide cells share a root
		const int tuftSide = desc.tuftBlades > 1 ? std::clamp(int(std::lround(std::sqrt(float(desc.tuftBlades)))), 2, g) : 1;
		for (uint32_t r = 0; r < blades; ++r)
		{
			const uint32_t m = reverse_bits(r, 2 * bits);
			uint32_t cx = 0, cz = 0;
			for (int b = 0; b < bits; ++b)
			{
				cx |= ((m >> (2 * b)) & 1u) << b;
				cz |= ((m >> (2 * b + 1)) & 1u) << b;
			}
			float x = (cx + random01(r * 4u + 1u)) * cell;
			float z = (cz + random01(r * 4u + 2u)) * cell;
			if (tuftSide > 1)
			{
				// in a tuft: the root of its square of tuftSide x tuftSide cells (jittered by the tuft), a few cm apart
				const uint32_t tx = cx / uint32_t(tuftSide), tz = cz / uint32_t(tuftSide);
				const uint32_t tuft = tz * uint32_t(g) + tx;
				const float span = cell * tuftSide;
				x = (tx + 0.2f + 0.6f * random01(tuft * 2u + 7001u)) * span + (random01(r * 4u + 1u) - 0.5f) * 0.04f;
				z = (tz + 0.2f + 0.6f * random01(tuft * 2u + 7002u)) * span + (random01(r * 4u + 2u) - 0.5f) * 0.04f;
			}
			const float rank = (r + 0.5f) / blades;
			const float r1 = random01(r * 4u + 3u), r2 = random01(r * 4u + 4u);
			const uint16_t base = uint16_t(vertices.size());
			vertices.push_back({ x, 0.0f, z, -1.0f, rank, r1, r2 });
			vertices.push_back({ x, 0.0f, z, 1.0f, rank, r1, r2 });
			vertices.push_back({ x, 0.55f, z, -1.0f, rank, r1, r2 });
			vertices.push_back({ x, 0.55f, z, 1.0f, rank, r1, r2 });
			vertices.push_back({ x, 1.0f, z, 0.0f, rank, r1, r2 });
			for (uint16_t i : { 0, 1, 2, 1, 3, 2, 2, 3, 4 }) indices.push_back(uint16_t(base + i));
			for (uint16_t i : { 0, 1, 4 }) indicesFar.push_back(uint16_t(base + i));
		}
		field->vertices = bgfx::createVertexBuffer(bgfx::copy(vertices.data(), uint32_t(vertices.size() * sizeof(GrassVertex))), grass_layout());
		field->indices = bgfx::createIndexBuffer(bgfx::copy(indices.data(), uint32_t(indices.size() * sizeof(uint16_t))));
		field->indicesFar = bgfx::createIndexBuffer(bgfx::copy(indicesFar.data(), uint32_t(indicesFar.size() * sizeof(uint16_t))));

		// Where it grows: the density map (or one full texel)
		const uint64_t sampleLinear = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
		const uint64_t samplePoint = sampleLinear | BGFX_SAMPLER_POINT;
		{
			const bool given = desc.densityWidth > 0 && desc.densityHeight > 0
				&& desc.density.size() >= size_t(desc.densityWidth) * size_t(desc.densityHeight);
			const int w = given ? desc.densityWidth : 1, h = given ? desc.densityHeight : 1;
			if (desc.densityWidth > 0 && !given) LOG_WARNING("[BgfxGrass] a density map smaller than its size: the field grows everywhere");
			const bgfx::Memory* mem = bgfx::alloc(uint32_t(w * h * 4));
			for (int i = 0; i < w * h; ++i)
			{
				mem->data[i * 4 + 0] = given ? desc.density[size_t(i)] : 255;
				mem->data[i * 4 + 1] = 0;
				mem->data[i * 4 + 2] = 0;
				mem->data[i * 4 + 3] = 255;
			}
			field->cover = bgfx::createTexture2D(uint16_t(w), uint16_t(h), false, 1, bgfx::TextureFormat::RGBA8, sampleLinear, mem);
		}
		// The ground's height, 16 bits over its range (point-sampled: vs_grass blends the four texels itself)
		{
			const bool given = desc.heightsWidth > 0 && desc.heightsHeight > 0
				&& desc.heights.size() >= size_t(desc.heightsWidth) * size_t(desc.heightsHeight);
			const int w = given ? desc.heightsWidth : 1, h = given ? desc.heightsHeight : 1;
			float low = desc.groundY, high = desc.groundY;
			if (given)
			{
				const auto [lo, hi] = std::minmax_element(desc.heights.begin(), desc.heights.begin() + size_t(w) * size_t(h));
				low = *lo;
				high = *hi;
			}
			field->heightLow = low;
			field->heightHigh = high;
			const float range = std::max(high - low, 0.0001f);
			const bgfx::Memory* mem = bgfx::alloc(uint32_t(w * h * 4));
			for (int i = 0; i < w * h; ++i)
			{
				const float v = given ? (desc.heights[size_t(i)] - low) / range : 0.0f;
				const uint32_t q = uint32_t(std::clamp(v, 0.0f, 1.0f) * 65535.0f + 0.5f);
				mem->data[i * 4 + 0] = uint8_t(q >> 8);
				mem->data[i * 4 + 1] = uint8_t(q & 0xFF);
				mem->data[i * 4 + 2] = 0;
				mem->data[i * 4 + 3] = 255;
			}
			field->ground = bgfx::createTexture2D(uint16_t(w), uint16_t(h), false, 1, bgfx::TextureFormat::RGBA8, samplePoint, mem);
		}
		if (!bgfx::isValid(field->vertices) || !bgfx::isValid(field->indices) || !bgfx::isValid(field->indicesFar) || !bgfx::isValid(field->cover) || !bgfx::isValid(field->ground))
		{
			LOG_WARNING("[BgfxGrass] a grass field's GPU resources could not be made");
			free_field(*field);
			return false;
		}
		LOG_INFO("[BgfxGrass] field %u: %.0f x %.0f m, tiles of %.1f m with %u blades", handle, desc.maxX - desc.minX,
			desc.maxZ - desc.minZ, tile, blades);
		m_fields[handle] = std::move(field);
		return true;
	}

	void BgfxGrass::remove(GraphicsSystem::GrassHandle handle)
	{
		auto it = m_fields.find(handle);
		if (it == m_fields.end()) return;
		free_field(*it->second);
		m_fields.erase(it);
	}

	uint32_t BgfxGrass::render(bgfx::ViewId viewId, const float eye[3], double seconds)
	{
		if (!available() || m_fields.empty() || m_densityScale <= 0.0f || m_reachScale <= 0.0f) return 0;
		PAKAL_PROFILE_SCOPE("gfx.grass");
		float planes[6][4];
		const bool culled = MeshComponent3D_Bgfx::main_frustum(planes);
		float focus[3];
		MeshComponent3D_Bgfx::scatter_origin(focus);
		float wind[2];
		MeshComponent3D_Bgfx::wind(wind);
		const float clock = float(std::fmod(seconds, 600.0));
		// the sun's shadow, read once at each blade's root (none: strength 0, and a texture bound all the same)
		bgfx::TextureHandle shadowMap = BGFX_INVALID_HANDLE;
		float shadowMatrix[16] = {}, shadowParams[4] = {};
		const bool shadows = MeshComponent3D_Bgfx::sun_shadow(shadowMap, shadowMatrix, shadowParams) && m_shadows;
		const float shadow4[4] = { shadows ? shadowParams[0] : 0.0f, shadowParams[1], 0.0f, 0.0f };
		if (!bgfx::isValid(shadowMap)) shadowMap = MeshComponent3D_Bgfx::white_texture();
		uint32_t drawn = 0;
		thread_local std::vector<float> rings[kRings];
		for (auto& [handle, fieldPtr] : m_fields)
		{
			const Field& field = *fieldPtr;
			const GraphicsSystem::GrassField& d = field.desc;
			const float reach = d.reach * m_reachScale;
			const float bend = std::max(0.0f, wind[0]) * d.heightMax;
			const float top = field.heightHigh + d.heightMax + 0.1f;
			// the tiles over the square around the focus, inside the field
			const float x0 = std::max(d.minX, focus[0] - reach), x1 = std::min(d.maxX, focus[0] + reach);
			const float z0 = std::max(d.minZ, focus[2] - reach), z1 = std::min(d.maxZ, focus[2] + reach);
			if (x0 >= x1 || z0 >= z1) continue;
			for (auto& ring : rings) ring.clear();
			const int i0 = int(std::floor((x0 - d.minX) / field.tile)), i1 = int(std::ceil((x1 - d.minX) / field.tile));
			const int j0 = int(std::floor((z0 - d.minZ) / field.tile)), j1 = int(std::ceil((z1 - d.minZ) / field.tile));
			for (int j = j0; j < j1; ++j)
			{
				for (int i = i0; i < i1; ++i)
				{
					const float tx = d.minX + i * field.tile, tz = d.minZ + j * field.tile;
					// its nearest point to the focus
					const float nx = std::clamp(focus[0], tx, tx + field.tile), nz = std::clamp(focus[2], tz, tz + field.tile);
					const float distance = std::sqrt((nx - focus[0]) * (nx - focus[0]) + (nz - focus[2]) * (nz - focus[2]));
					if (distance > reach) continue;
					if (culled)
					{
						const float lo[3] = { tx - bend, field.heightLow, tz - bend };
						const float hi[3] = { tx + field.tile + bend, top, tz + field.tile + bend };
						bool outside = false;
						for (int p = 0; p < 6 && !outside; ++p)
						{
							// the box's corner farthest along the plane's normal
							const float px = planes[p][0] >= 0.0f ? hi[0] : lo[0];
							const float py = planes[p][1] >= 0.0f ? hi[1] : lo[1];
							const float pz = planes[p][2] >= 0.0f ? hi[2] : lo[2];
							outside = planes[p][0] * px + planes[p][1] * py + planes[p][2] * pz + planes[p][3] < 0.0f;
						}
						if (outside) continue;
					}
					int ring = 0;
					while (ring < kRings - 1 && distance > kRingEdge[ring] * reach) ++ring;
					rings[ring].push_back(tx);
					rings[ring].push_back(tz);
				}
			}

			const float field4[4] = { d.minX, d.minZ, 1.0f / (d.maxX - d.minX), 1.0f / (d.maxZ - d.minZ) };
			const float shape4[4] = { d.heightMin, d.heightMax, d.width, d.groundY };
			const float reach4[4] = { reach, m_densityScale, field.tile, std::min(3.0f, 0.15f * reach) };
			const float ground4[4] = { float(std::max(1, d.heightsWidth)), float(std::max(1, d.heightsHeight)), field.heightLow,
				field.heightHigh - field.heightLow };
			const float focus4[4] = { focus[0], focus[1], focus[2], clock };
			const float eye4[4] = { eye[0], eye[1], eye[2], 0.0f };
			const float wind4[4] = { wind[0], wind[1], 0.0f, wind[0] > 0.0f ? 1.0f : 0.0f };
			const float root4[4] = { d.colorRoot[0], d.colorRoot[1], d.colorRoot[2], 1.0f };
			const float tip4[4] = { d.colorTip[0], d.colorTip[1], d.colorTip[2], 1.0f };
			float clumpColors[3][4];
			for (int k = 0; k < 3; ++k)
			{
				for (int c = 0; c < 3; ++c) clumpColors[k][c] = d.clumpColors[k].color[c];
				clumpColors[k][3] = std::clamp(d.clumpColors[k].share, 0.0f, 1.0f);
			}
			const float dead4[4] = { d.colorDead[0], d.colorDead[1], d.colorDead[2], std::clamp(d.deadShare, 0.0f, 1.0f) };
			const float clump4[4] = { 1.0f / std::max(0.2f, d.clumpSize), std::clamp(d.clumpStrength, 0.0f, 1.0f),
				float(std::max(0, d.tuftBlades)), std::clamp(d.tuftSpread, 0.0f, 1.0f) };
			const float thin4[4] = { std::clamp(d.thinFrom, 0.0f, 0.95f), std::clamp(d.farDensity, 0.0f, 1.0f), 0.0f, 0.0f };
			for (int ring = 0; ring < kRings; ++ring)
			{
				const uint32_t tiles = uint32_t(rings[ring].size() / 2);
				if (tiles == 0) continue;
				// as many blades as the density needs at the ring's nearest edge
				const float nearest = ring == 0 ? 0.0f : kRingEdge[ring - 1] * reach;
				const uint32_t blades = std::min(field.blades, uint32_t(std::ceil(density_at(nearest, reach, m_densityScale, d.thinFrom, d.farDensity) * field.blades)));
				if (blades == 0) continue;
				const uint16_t stride = 16;
				if (bgfx::getAvailInstanceDataBuffer(tiles, stride) < tiles) break;
				bgfx::InstanceDataBuffer instances;
				bgfx::allocInstanceDataBuffer(&instances, tiles, stride);
				float* out = reinterpret_cast<float*>(instances.data);
				for (uint32_t k = 0; k < tiles; ++k)
				{
					out[0] = rings[ring][k * 2];
					out[1] = rings[ring][k * 2 + 1];
					out[2] = 0.0f;
					out[3] = 0.0f;
					out += 4;
				}
				bgfx::setUniform(m_uField, field4);
				bgfx::setUniform(m_uShape, shape4);
				bgfx::setUniform(m_uReach, reach4);
				bgfx::setUniform(m_uGround, ground4);
				bgfx::setUniform(m_uFocus, focus4);
				bgfx::setUniform(m_uEye, eye4);
				bgfx::setUniform(m_uWind, wind4);
				bgfx::setUniform(m_uRoot, root4);
				bgfx::setUniform(m_uTip, tip4);
				bgfx::setUniform(m_uClumpColors, clumpColors, 3);
				bgfx::setUniform(m_uDead, dead4);
				bgfx::setUniform(m_uClump, clump4);
				bgfx::setUniform(m_uThin, thin4);
				bgfx::setUniform(m_uShadowMtx, shadowMatrix);
				bgfx::setUniform(m_uShadow, shadow4);
				bgfx::setTexture(2, m_sShadow, shadowMap);
				bgfx::setTexture(0, m_sCover, field.cover);
				bgfx::setTexture(1, m_sGround, field.ground);
				bgfx::setInstanceDataBuffer(&instances);
				bgfx::setVertexBuffer(0, field.vertices);
				if (ring >= kFarRing) bgfx::setIndexBuffer(field.indicesFar, 0, blades * kBladeIndicesFar);
				else bgfx::setIndexBuffer(field.indices, 0, blades * kBladeIndices);
				// solid and double-sided: the GPU's hidden surface removal culls what is behind
				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA);
				// nearest rings first (the main view sorts by this depth)
				bgfx::submit(viewId, m_program, uint32_t(ring));
				drawn += blades * tiles;
			}
		}
		return drawn;
	}
}
