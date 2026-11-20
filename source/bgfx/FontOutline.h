///////////////////////////////////////////////////////////////////////////////////////////////////////
// File: FontOutline.h
// Purpose: A glyph's outline (what RmlUi's font-effect: outline draws) in time that does not grow
//          with its width: the distance of each pixel to the glyph (an exact Euclidean distance
//          transform, Felzenszwalb and Huttenlocher's) instead of RmlUi's dilation, which visits a
//          whole disc of (2w+1)^2 pixels for each one. The same picture: full within the width, a
//          pixel of soft edge beyond it. A title's letters (200 dp, a 6 dp outline: hundreds of
//          pixels a side) took a phone's CPU a second each; this takes milliseconds.
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Pakal
{
	namespace detail
	{
		// where past the outline's width its soft edge is: RmlUi's falls off over the pixel beyond the width
		inline constexpr float kEdgeDefault = 0.65f;   // (measured against RmlUi's: tests/FontOutlineTests.cpp)
		// The squared distance transform of one line of 'n' values (0 on the glyph, a large number off it), in place; 'nearest'
		// (n, its own 'step' apart) gets the place on the line each one's distance is measured to. 'v', 'z' and 'out': scratch
		// of n + 1, n + 2 and n
		inline void distance_line(float* f, int n, int step, int* nearest, std::vector<int>& v, std::vector<float>& z, std::vector<float>& out)
		{
			constexpr float kFar = 1e20f;
			int k = 0;
			v[0] = 0;
			z[0] = -kFar;
			z[1] = kFar;
			for (int q = 1; q < n; ++q)
			{
				const float fq = f[q * step];
				float s = ((fq + float(q) * float(q)) - (f[v[k] * step] + float(v[k]) * float(v[k]))) / (2.0f * float(q - v[k]));
				while (s <= z[k])
				{
					--k;
					s = ((fq + float(q) * float(q)) - (f[v[k] * step] + float(v[k]) * float(v[k]))) / (2.0f * float(q - v[k]));
				}
				++k;
				v[k] = q;
				z[k] = s;
				z[k + 1] = kFar;
			}
			k = 0;
			for (int q = 0; q < n; ++q)
			{
				while (z[k + 1] < float(q)) ++k;
				const float d = float(q - v[k]);
				out[q] = d * d + f[v[k] * step];
				nearest[q * step] = v[k];
			}
			for (int q = 0; q < n; ++q) f[q * step] = out[q];
		}
	}

	// The outline of a glyph of 'width' x 'height' pixels (its coverage: the byte at 'alphaOffset' of each pixel of
	// 'sourceBytes' bytes, rows 'sourceStride' apart) 'radius' pixels wide, into a picture 2 * radius larger each way
	// (its coverage written at 'destAlphaOffset' of each pixel of 'destBytes' bytes; the other bytes are left alone)
	// (up to this width, RmlUi's own way: the same bytes)
	inline constexpr int kExactRadius = 3;

	inline void outline_glyph(const uint8_t* source, int width, int height, int sourceStride, int sourceBytes, int alphaOffset,
		uint8_t* dest, int destStride, int destBytes, int destAlphaOffset, int radius)
	{
		const int w = width + 2 * radius, h = height + 2 * radius;
		if (w <= 0 || h <= 0) return;
		auto coverage = [&](int x, int y) { return source[y * sourceStride + x * sourceBytes + alphaOffset]; };
		// the glyph: its pixels at least half covered (or, a glyph that thin, half of its most covered)
		int most = 0;
		for (int y = 0; y < height; ++y)
			for (int x = 0; x < width; ++x) most = std::max<int>(most, coverage(x, y));
		if (most == 0)
		{
			for (int y = 0; y < h; ++y)
				for (int x = 0; x < w; ++x) dest[y * destStride + x * destBytes + destAlphaOffset] = 0;
			return;
		}
		// a thin outline: RmlUi's own way (the most of each pixel's coverage times a disc's weight), which costs little that thin
		if (radius <= kExactRadius)
		{
			const int side = 2 * radius + 1;
			std::vector<float> weight(size_t(side) * size_t(side));
			for (int ky = -radius; ky <= radius; ++ky)
				for (int kx = -radius; kx <= radius; ++kx)
				{
					const float distance = std::sqrt(float(kx * kx + ky * ky));
					weight[size_t(ky + radius) * size_t(side) + size_t(kx + radius)] = distance > float(radius) ? std::max(0.0f, float(radius) + 1.0f - distance) : 1.0f;
				}
			for (int y = 0; y < h; ++y)
				for (int x = 0; x < w; ++x)
				{
					float best = 0.0f;
					for (int ky = -radius; ky <= radius; ++ky)
					{
						const int gy = y - radius + ky;
						if (gy < 0 || gy >= height) continue;
						for (int kx = -radius; kx <= radius; ++kx)
						{
							const int gx = x - radius + kx;
							if (gx < 0 || gx >= width) continue;
							best = std::max(best, float(coverage(gx, gy)) * weight[size_t(ky + radius) * size_t(side) + size_t(kx + radius)]);
						}
					}
					dest[y * destStride + x * destBytes + destAlphaOffset] = uint8_t(std::min(255.0f, best));
				}
			return;
		}
		const int inside = std::min(128, (most + 1) / 2);
		constexpr float kFar = 1e20f;
		const float kEdge = detail::kEdgeDefault;
		std::vector<float> d(size_t(w) * size_t(h), kFar);
		for (int y = 0; y < height; ++y)
			for (int x = 0; x < width; ++x)
				if (coverage(x, y) >= inside) d[size_t(y + radius) * size_t(w) + size_t(x + radius)] = 0.0f;
		const int n = std::max(w, h);
		std::vector<int> v(static_cast<size_t>(n) + 1), row(size_t(w) * size_t(h)), column(size_t(w) * size_t(h));
		std::vector<float> z(static_cast<size_t>(n) + 2), out(static_cast<size_t>(n));
		// down each column (the row of the glyph's nearest pixel in it), then along each row (the column whose nearest is nearest)
		for (int x = 0; x < w; ++x) detail::distance_line(&d[size_t(x)], h, w, &row[size_t(x)], v, z, out);
		for (int y = 0; y < h; ++y) detail::distance_line(&d[size_t(y) * size_t(w)], w, 1, &column[size_t(y) * size_t(w)], v, z, out);
		// full within the radius, then a pixel of soft edge; a pixel is never less covered than the glyph itself is there. The
		// distance is to the glyph's edge, not to its nearest pixel's centre: that pixel's own coverage says how far past its
		// centre the edge runs (half covered: through it)
		for (int y = 0; y < h; ++y)
			for (int x = 0; x < w; ++x)
			{
				const size_t at = size_t(y) * size_t(w) + size_t(x);
				const int sx = column[at], sy = row[size_t(y) * size_t(w) + size_t(sx)];
				const float dx = float(x - sx), dy = float(y - sy);
				const float edge = float(coverage(sx - radius, sy - radius)) / 255.0f - 0.5f;
				const float a = std::clamp(float(radius) + kEdge - (std::sqrt(dx * dx + dy * dy) - edge), 0.0f, 1.0f);
				int value = int(a * 255.0f + 0.5f);
				const int gx = x - radius, gy = y - radius;
				if (gx >= 0 && gy >= 0 && gx < width && gy < height) value = std::max<int>(value, coverage(gx, gy));
				dest[y * destStride + x * destBytes + destAlphaOffset] = uint8_t(value);
			}
	}
}
