// FontOutline.h against RmlUi's own outline (its dilation, copied here: the most of each pixel's coverage times a disc's weight,
// 1 within the width and falling to 0 a pixel beyond it): the same picture, and how much faster
#include "bgfx/FontOutline.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{
	void rmlui_outline(const std::vector<uint8_t>& glyph, int width, int height, int radius, std::vector<uint8_t>& out)
	{
		const int w = width + 2 * radius, h = height + 2 * radius;
		out.assign(size_t(w) * size_t(h), 0);
		std::vector<float> weight(size_t(2 * radius + 1) * size_t(2 * radius + 1));
		for (int y = -radius; y <= radius; ++y)
			for (int x = -radius; x <= radius; ++x)
			{
				const float distance = std::sqrt(float(x * x + y * y));
				weight[size_t(y + radius) * size_t(2 * radius + 1) + size_t(x + radius)] = distance > radius ? std::max(0.0f, radius + 1 - distance) : 1.0f;
			}
		for (int y = 0; y < h; ++y)
			for (int x = 0; x < w; ++x)
			{
				float best = 0;
				for (int ky = -radius; ky <= radius; ++ky)
					for (int kx = -radius; kx <= radius; ++kx)
					{
						const int gx = x - radius + kx, gy = y - radius + ky;
						if (gx < 0 || gy < 0 || gx >= width || gy >= height) continue;
						best = std::max(best, glyph[size_t(gy) * size_t(width) + size_t(gx)] * weight[size_t(ky + radius) * size_t(2 * radius + 1) + size_t(kx + radius)]);
					}
				out[size_t(y) * size_t(w) + size_t(x)] = uint8_t(std::min(255.0f, best));
			}
	}

	// A letter-like shape, smooth-edged (coverage from the distance to its edge): a ring with a bar through it
	std::vector<uint8_t> make_glyph(int size)
	{
		std::vector<uint8_t> g(size_t(size) * size_t(size));
		const float c = size * 0.5f, outer = size * 0.42f, inner = size * 0.26f;
		for (int y = 0; y < size; ++y)
			for (int x = 0; x < size; ++x)
			{
				const float px = x + 0.5f - c, py = y + 0.5f - c, r = std::sqrt(px * px + py * py);
				float sd = std::max(r - outer, inner - r);                                  // the ring (negative inside)
				sd = std::min(sd, std::max(std::fabs(py) - size * 0.06f, std::fabs(px) - outer));  // and the bar
				g[size_t(y) * size_t(size) + size_t(x)] = uint8_t(std::clamp(0.5f - sd, 0.0f, 1.0f) * 255.0f + 0.5f);
			}
		return g;
	}
}

int main()
{
	int failures = 0;
	for (const auto& [size, radius] : { std::pair{ 40, 2 }, std::pair{ 60, 3 }, std::pair{ 120, 6 }, std::pair{ 200, 12 }, std::pair{ 400, 12 } })
	{
		const auto glyph = make_glyph(size);
		std::vector<uint8_t> reference, fast(size_t(size + 2 * radius) * size_t(size + 2 * radius));
		const auto t0 = std::chrono::steady_clock::now();
		rmlui_outline(glyph, size, size, radius, reference);
		const auto t1 = std::chrono::steady_clock::now();
		Pakal::outline_glyph(glyph.data(), size, size, size, 1, 0, fast.data(), size + 2 * radius, 1, 0, radius);
		const auto t2 = std::chrono::steady_clock::now();
		int worst = 0;
		double sum = 0;
		size_t off = 0;
		for (size_t i = 0; i < fast.size(); ++i)
		{
			const int e = std::abs(int(fast[i]) - int(reference[i]));
			worst = std::max(worst, e);
			sum += e;
			off += e > 64;
		}
		const double rml = std::chrono::duration<double, std::milli>(t1 - t0).count(), ours = std::chrono::duration<double, std::milli>(t2 - t1).count();
		// a thin outline is RmlUi's own (the same bytes); a wide one, the same picture: off only at the edge's antialiasing, and few
		const bool same = radius <= Pakal::kExactRadius ? worst == 0 : sum / double(fast.size()) < 2.0 && off * 200 < fast.size();
		std::printf("[%s] %dpx glyph, %dpx outline: mean error %.2f, worst %d, %zu pixels off by more than a quarter; RmlUi %.1f ms, ours %.2f ms\n",
			same ? "PASS" : "FAIL", size, radius, sum / double(fast.size()), worst, off, rml, ours);
		if (!same) ++failures;
	}
	return failures == 0 ? 0 : 1;
}
