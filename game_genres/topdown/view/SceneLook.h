///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SceneLook.h
// Purpose: How a game's scene looks: its light (the sun and its shadows, the sky's and the ground's
//          ambient, the plastic gloss) and its picture (grading, tilt-shift, occlusion), from a level's
//          "lighting" and "post" sections. A game reads it once and applies it on the graphics thread.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "bgfx/BgfxGraphicsSystem.h"

#include <nlohmann/json_fwd.hpp>

namespace Pakal::TopDown
{
	struct SceneLook
	{
		static constexpr float kDefaultGloss = 0.35f;     // the plastic highlight (MeshComponent3D_Bgfx::set_style)...
		static constexpr float kGlossShininess = 40.0f;   // ... and how tight it is

		float sunDirection[3] = { 0.4f, -1.0f, -0.6f };
		float sunColor[3] = { 1.0f, 0.95f, 0.85f };
		float ambient[3] = { 0.25f, 0.25f, 0.3f };
		float ambientGround[3] = { 0.16f, 0.15f, 0.14f };
		float skyTop[3] = { 0.25f, 0.45f, 0.75f };
		float skyHorizon[3] = { 0.75f, 0.82f, 0.9f };
		float skyClouds = 0.55f;   // lighting.skyClouds: how much of the sky the clouds cover (0: clear)
		float gloss = kDefaultGloss;
		Pakal::BgfxGraphicsSystem::ShadowSettings shadows;
		Pakal::BgfxGraphicsSystem::PostSettings post;

		// The sections over the engine's defaults (what they leave out keeps them)
		static SceneLook from_json(const nlohmann::json& lighting, const nlohmann::json& post);
		// Sets them all (on the graphics thread)
		void apply(Pakal::BgfxGraphicsSystem& graphics) const;
	};
}
