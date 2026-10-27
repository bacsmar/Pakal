///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SceneLook.cpp
// Purpose: How a game's scene looks - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/view/SceneLook.h"
#include "bgfx/MeshComponent3D_Bgfx.h"

#include <nlohmann/json.hpp>

namespace Pakal::TopDown
{
	namespace
	{
		void read_floats(const nlohmann::json& value, float* out, size_t count)
		{
			if (value.is_array()) for (size_t i = 0; i < count && i < value.size(); ++i) out[i] = value[i].get<float>();
		}
	}

	SceneLook SceneLook::from_json(const nlohmann::json& lighting, const nlohmann::json& postJson)
	{
		SceneLook look;
		const nlohmann::json json = lighting.is_object() ? lighting : nlohmann::json::object();
		read_floats(json.value("sunDirection", nlohmann::json()), look.sunDirection, 3);
		read_floats(json.value("sunColor", nlohmann::json()), look.sunColor, 3);
		read_floats(json.value("ambient", nlohmann::json()), look.ambient, 3);
		read_floats(json.value("ambientGround", nlohmann::json()), look.ambientGround, 3);
		read_floats(json.value("skyTop", nlohmann::json()), look.skyTop, 3);
		read_floats(json.value("skyHorizon", nlohmann::json()), look.skyHorizon, 3);
		look.gloss = json.value("gloss", kDefaultGloss);

		auto& shadows = look.shadows;
		const nlohmann::json sj = json.value("shadows", nlohmann::json::object());
		shadows.enabled = sj.value("enabled", false);
		shadows.mapSize = sj.value("mapSize", shadows.mapSize);
		shadows.extent = sj.value("extent", shadows.extent);
		shadows.strength = sj.value("strength", shadows.strength);
		shadows.bias = sj.value("bias", shadows.bias);
		shadows.normalOffset = sj.value("normalOffset", shadows.normalOffset);

		auto& post = look.post;
		const nlohmann::json pj = postJson.is_object() ? postJson : nlohmann::json::object();
		post.enabled = pj.value("enabled", false);
		post.exposure = pj.value("exposure", post.exposure);
		post.contrast = pj.value("contrast", post.contrast);
		post.saturation = pj.value("saturation", post.saturation);
		read_floats(pj.value("gain", nlohmann::json()), post.gain, 3);
		post.vignette = pj.value("vignette", post.vignette);
		post.tiltBlur = pj.value("tiltBlur", post.tiltBlur);
		post.tiltFocus = pj.value("tiltFocus", post.tiltFocus);
		post.tiltBand = pj.value("tiltBand", post.tiltBand);
		post.aoStrength = pj.value("aoStrength", post.aoStrength);
		post.aoRadius = pj.value("aoRadius", post.aoRadius);
		post.aoBias = pj.value("aoBias", post.aoBias);
		post.aoRange = pj.value("aoRange", post.aoRange);
		return look;
	}

	void SceneLook::apply(Pakal::BgfxGraphicsSystem& graphics) const
	{
		graphics.set_directional_light(sunDirection[0], sunDirection[1], sunDirection[2], sunColor[0], sunColor[1], sunColor[2]);
		graphics.set_ambient_light(ambient[0], ambient[1], ambient[2]);
		graphics.set_ground_ambient_light(ambientGround[0], ambientGround[1], ambientGround[2]);
		graphics.set_sky_colors(skyTop[0], skyTop[1], skyTop[2], skyHorizon[0], skyHorizon[1], skyHorizon[2]);
		Pakal::MeshComponent3D_Bgfx::set_style(gloss, kGlossShininess);
		graphics.set_shadows(shadows);
		graphics.set_post_process(post);
	}
}
