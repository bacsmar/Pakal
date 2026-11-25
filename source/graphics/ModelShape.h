///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ModelShape.h
// Purpose: A loaded model's shape, described without the renderer (ADR-0017): its box, the lowest point
//          of a crown split off at load, and its columns seen from above. The graphics backend fills it
//          when it loads a model; genres read this, never the backend's mesh class.
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

namespace Pakal
{
	struct ModelShape
	{
		// A tall static model seen from above, in columns (64 a side at most, over its box's x and z): in each, the lowest
		// and highest point of its surfaces, in model units. What hides the player is tested against it: a box alone takes
		// in the air beside an awning or under a sign.
		struct Columns
		{
			float x0 = 0.0f, z0 = 0.0f, cell = 1.0f;
			int nx = 0, nz = 0;
			std::vector<float> low, high;   // per column (row-major along z); low > high: nothing there
			// whether (x, y, z), model units, is within 'margin' of the surfaces of its column
			bool solid_at(float x, float y, float z, float margin) const
			{
				const int i = int(std::floor((x - x0) / cell)), k = int(std::floor((z - z0) / cell));
				if (i < 0 || k < 0 || i >= nx || k >= nz) return false;
				const size_t c = size_t(k) * size_t(nx) + size_t(i);
				return low[c] <= high[c] && y >= low[c] - margin && y <= high[c] + margin;
			}
		};

		// The model's box as loaded (bind pose for a skinned one), before its placement; model units
		float lo[3] = { 0.0f, 0.0f, 0.0f }, hi[3] = { 0.0f, 0.0f, 0.0f };
		// The lowest point of the crown split off at load (a tree's leaf cards wholly above a third of its height, a part of
		// their own); NaN: no crown was split
		float crownBase = std::numeric_limits<float>::quiet_NaN();
		// Its columns (static models 2 units tall or more), or null
		std::shared_ptr<const Columns> columns;

		bool has_crown() const { return !std::isnan(crownBase); }
	};
}
