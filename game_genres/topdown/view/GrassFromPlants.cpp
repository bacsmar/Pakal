///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: GrassFromPlants.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/view/GrassFromPlants.h"

#include <algorithm>
#include <cmath>

namespace Pakal::TopDown
{
	GrassFromPlants::GrassFromPlants(const nlohmann::json& spec)
	{
		if (!spec.is_object()) return;
		m_spec = spec;
		const nlohmann::json replaces = spec.value("replaces", nlohmann::json::array());
		if (!replaces.is_array()) return;
		for (const nlohmann::json& prefix : replaces)
		{
			if (prefix.is_string() && !prefix.get<std::string>().empty()) m_replaces.push_back(prefix.get<std::string>());
		}
	}

	bool GrassFromPlants::replaces(const std::string& model) const
	{
		const size_t slash = model.find_last_of("/\\");
		const std::string name = model.substr(slash == std::string::npos ? 0 : slash + 1);
		for (const std::string& prefix : m_replaces)
		{
			if (name.rfind(prefix, 0) == 0) return true;
		}
		return false;
	}

	Pakal::GraphicsSystem::GrassField GrassFromPlants::field() const
	{
		return field_from(m_spec, m_places);
	}

	Pakal::GraphicsSystem::GrassField GrassFromPlants::field_from(const nlohmann::json& spec, const std::vector<std::array<float, 3>>& places)
	{
		Pakal::GraphicsSystem::GrassField field;
		if (places.empty()) return field;
		const nlohmann::json s = spec.is_object() ? spec : nlohmann::json::object();
		// the places' rectangle, in cells of 'cell' meters
		float lo[2] = { places[0][0], places[0][2] }, hi[2] = { lo[0], lo[1] };
		for (const auto& p : places)
		{
			lo[0] = std::min(lo[0], p[0]);
			hi[0] = std::max(hi[0], p[0]);
			lo[1] = std::min(lo[1], p[2]);
			hi[1] = std::max(hi[1], p[2]);
		}
		const float cell = std::max(0.5f, s.value("cell", 2.0f));
		// a margin of two cells all round: the softening below reaches two cells past the last plant
		lo[0] -= 2.0f * cell;
		lo[1] -= 2.0f * cell;
		hi[0] += 2.0f * cell;
		hi[1] += 2.0f * cell;
		const int w = std::max(1, static_cast<int>((hi[0] - lo[0]) / cell) + 1);
		const int h = std::max(1, static_cast<int>((hi[1] - lo[1]) / cell) + 1);
		std::vector<float> count(size_t(w) * size_t(h), 0.0f), sumY(count.size(), 0.0f);
		double meanY = 0.0;
		for (const auto& p : places)
		{
			const int cx = std::clamp(static_cast<int>((p[0] - lo[0]) / cell), 0, w - 1);
			const int cz = std::clamp(static_cast<int>((p[2] - lo[1]) / cell), 0, h - 1);
			const size_t i = size_t(cz) * size_t(w) + size_t(cx);
			count[i] += 1.0f;
			sumY[i] += p[1];
			meanY += p[1];
		}
		meanY /= double(places.size());
		// the density: the cells with a plant, softened over 5 x 5 cells (a patch thins toward its edges), full at 'full'
		// of them (0.3: a plant in about a third of the cells around is a full lawn)
		const float full = std::max(0.01f, s.value("full", 0.3f));
		field.minX = lo[0];
		field.minZ = lo[1];
		field.maxX = lo[0] + float(w) * cell;
		field.maxZ = lo[1] + float(h) * cell;
		field.density.resize(count.size());
		field.heights.resize(count.size());
		// the ground's height: the mean of the plants in a cell, else of those around it (a soft fill), else the mean of all
		for (int z = 0; z < h; ++z)
		{
			for (int x = 0; x < w; ++x)
			{
				float sum = 0.0f, near = 0.0f, nearY = 0.0f;
				for (int dz = -2; dz <= 2; ++dz)
				{
					for (int dx = -2; dx <= 2; ++dx)
					{
						const int nx = x + dx, nz = z + dz;
						if (nx < 0 || nz < 0 || nx >= w || nz >= h) continue;
						const size_t j = size_t(nz) * size_t(w) + size_t(nx);
						sum += std::min(count[j], 1.0f);
						near += count[j];
						nearY += sumY[j];
					}
				}
				const size_t i = size_t(z) * size_t(w) + size_t(x);
				field.density[i] = static_cast<uint8_t>(std::lround(std::clamp(sum / 25.0f / full, 0.0f, 1.0f) * 255.0f));
				field.heights[i] = count[i] > 0.0f ? sumY[i] / count[i] : (near > 0.0f ? nearY / near : float(meanY));
			}
		}
		field.densityWidth = field.heightsWidth = w;
		field.densityHeight = field.heightsHeight = h;
		field.groundY = float(meanY);
		apply_look(s, field);
		return field;
	}

	void GrassFromPlants::apply_look(const nlohmann::json& spec, Pakal::GraphicsSystem::GrassField& field)
	{
		if (!spec.is_object()) return;
		auto color = [&spec](const char* key, float out[3])
		{
			const nlohmann::json c = spec.value(key, nlohmann::json::array());
			if (!c.is_array() || c.size() != 3) return;
			for (int i = 0; i < 3; ++i)
			{
				if (c[size_t(i)].is_number()) out[i] = c[size_t(i)].get<float>();
			}
		};
		const nlohmann::json heights = spec.value("heights", nlohmann::json::array());
		if (heights.is_array() && heights.size() == 2 && heights[0].is_number() && heights[1].is_number())
		{
			field.heightMin = heights[0].get<float>();
			field.heightMax = std::max(field.heightMin, heights[1].get<float>());
		}
		field.width = spec.value("width", field.width);
		field.bladesPerSquareMeter = spec.value("bladesPerSquareMeter", field.bladesPerSquareMeter);
		field.reach = spec.value("reach", field.reach);
		field.thinFrom = spec.value("thinFrom", field.thinFrom);
		field.farDensity = spec.value("farDensity", field.farDensity);
		color("root", field.colorRoot);
		color("tip", field.colorTip);
		color("dead", field.colorDead);
		field.deadShare = spec.value("deadShare", field.deadShare);
		field.clumpSize = spec.value("clumpSize", field.clumpSize);
		field.clumpStrength = spec.value("clumpStrength", field.clumpStrength);
		const nlohmann::json clumps = spec.value("clumpColors", nlohmann::json());
		if (clumps.is_array())
		{
			// given: they replace the defaults (the ones not given take no share)
			for (size_t k = 0; k < 3; ++k)
			{
				field.clumpColors[k].share = 0.0f;
				if (k >= clumps.size() || !clumps[k].is_object()) continue;
				const nlohmann::json c = clumps[k].value("color", nlohmann::json::array());
				if (c.is_array() && c.size() == 3)
				{
					for (int i = 0; i < 3; ++i) field.clumpColors[k].color[i] = c[size_t(i)].get<float>();
				}
				field.clumpColors[k].share = clumps[k].value("share", 0.0f);
			}
		}
		field.tuftBlades = spec.value("tuftBlades", field.tuftBlades);
		field.tuftSpread = spec.value("tuftSpread", field.tuftSpread);
	}
}
