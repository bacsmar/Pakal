///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: GrassFromPlants.h
// Purpose: A field of GPU grass (GraphicsSystem::create_grass, docs/GRASS.md) in place of a level's
//          grass models: the plants whose model name starts with one of the spec's "replaces" are not
//          drawn as models, and their places give the field its density (how many in each cell,
//          softened so a patch thins toward its edges) and its ground height. The blades' look comes
//          from the same spec. No graphics here: a level loader collects the places and makes the field.
//
//          The spec: { "replaces": ["Grass_Short", "Grass_Med"], "cell": 2, "full": 0.3,
//                      "heights": [0.2, 0.5], "width", "bladesPerSquareMeter", "reach", "root", "tip",
//                      "dead", "deadShare", "clumpSize", "clumpStrength", "clumpColors": [{color, share}],
//                      "tuftBlades", "tuftSpread", "thinFrom", "farDensity" }
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "GraphicsSystem.h"

#include <array>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace Pakal::TopDown
{
	class GrassFromPlants
	{
	public:
		GrassFromPlants() = default;
		// The spec above (an object without "replaces": none)
		explicit GrassFromPlants(const nlohmann::json& spec);

		bool empty() const { return m_replaces.empty(); }
		// Whether a model (its file or its name) is one the field replaces: the loader skips it and calls add()
		bool replaces(const std::string& model) const;
		// A replaced plant's place, render space (x, the ground's height, z)
		void add(float x, float y, float z) { m_places.push_back({ x, y, z }); }
		size_t plants() const { return m_places.size(); }

		// The field over the places added (empty density when there are none: make no field then)
		Pakal::GraphicsSystem::GrassField field() const;

		// The same from a spec and places, for a loader that keeps its own list
		static Pakal::GraphicsSystem::GrassField field_from(const nlohmann::json& spec, const std::vector<std::array<float, 3>>& places);
		// The blades' look of a spec on a field (its density and heights untouched)
		static void apply_look(const nlohmann::json& spec, Pakal::GraphicsSystem::GrassField& field);

	private:
		std::vector<std::string> m_replaces;
		std::vector<std::array<float, 3>> m_places;
		nlohmann::json m_spec;
	};
}
