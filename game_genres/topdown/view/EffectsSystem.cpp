///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: EffectsSystem.cpp
// Purpose: The look of shooting - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/view/EffectsSystem.h"
#include "topdown/ActorOrientation.h"

#include "PakalMath.h"
#include "bgfx/BgfxGraphicsSystem.h"
#include "bgfx/MeshComponent3D_Bgfx.h"

#include <algorithm>
#include <cmath>

namespace Pakal::TopDown
{
	namespace
	{
		const size_t kBulletModels = 24;
		const size_t kFlashModels = 12;
		const size_t kCasingModels = 14;
		const float kGunHeight = 1.0f;          // meters above the floor where bullets fly
		const float kMuzzleScale = 1.0f;        // the game scales its flash by 0.7; the quad here is 1.1 m wide
		const float kMuzzleLife = 0.09f;
		const float kMuzzleAhead = 0.28f;       // the flash starts in front of the gun
		const float kCasingScale = 0.13f;
		const float kGravity = 9.8f;
		const float kCasingLife = 1.0f;
		const float kSparkGravity = 6.0f;
		const size_t kMostPuffs = 1500;         // particles alive at once (the oldest effects stop adding past it)
		// The muzzle lights the floor around the shooter for a moment each shot (the original's ground glow, ~3 m)
		const float kMuzzleLightRadius = 2.6f, kMuzzleLightLife = 0.07f;
		// Blood on the floor stays this long, fading out over its last seconds; at most this many marks at once
		const float kStainLife = 40.0f, kStainFade = 6.0f;
		const size_t kMostStains = 160;
		const float kStainHeight = 0.03f;       // just over the floor
	}

	EffectsSystem::EffectsSystem(Pakal::BgfxGraphicsSystem* graphics, const MeshFactory& make, const std::string& bullet,
		const std::string& muzzle, const std::string& shell, uint32_t seed) :
		m_graphics(graphics),
		m_random(seed)
	{
		m_make = make;   // (kept for the pieces stocked later: stock_pieces, set_casings)
		m_shell = shell;
		m_casingScale = kCasingScale;
		m_casingLife = kCasingLife;
		auto hide = [](Pakal::MeshComponent3D_Bgfx& mesh) { mesh.set_scale(0.0f); };
		if (!bullet.empty())
		{
			for (size_t i = 0; i < kBulletModels; ++i)
			{
				if (auto mesh = make(bullet)) { hide(*mesh); m_bullets.push_back(std::move(mesh)); }
			}
		}
		if (!muzzle.empty())
		{
			for (size_t i = 0; i < kFlashModels; ++i)
			{
				Flash flash;
				flash.mesh = make(muzzle);
				if (flash.mesh) { hide(*flash.mesh); m_flashes.push_back(std::move(flash)); }
			}
		}
		if (!shell.empty())
		{
			for (size_t i = 0; i < kCasingModels; ++i)
			{
				Casing casing;
				casing.mesh = make(shell);
				if (casing.mesh) { hide(*casing.mesh); m_casings.push_back(std::move(casing)); }
			}
		}
	}

	EffectsSystem::~EffectsSystem() = default;

	void EffectsSystem::release()
	{
		m_bullets.clear();
		m_flashes.clear();
		m_casings.clear();
		m_pieces.clear();
		m_pieceSets = 0;
		if (m_graphics) m_graphics->clear_gpu_particles();   // none of this level's left over the next screen
	}

	float EffectsSystem::random(float low, float high)
	{
		return m_random.uniform(low, high);
	}

	void EffectsSystem::start_flash(float x, float y, float height, float yaw, float scale, float life, float r, float g, float b)
	{
		for (auto& flash : m_flashes)
		{
			if (flash.age >= 0.0f)
			{
				continue;
			}
			flash.age = 0.0f;
			flash.life = life;
			flash.scale = scale;
			flash.x = x;
			flash.y = y;
			flash.height = height;
			flash.yaw = yaw;
			flash.color[0] = r;
			flash.color[1] = g;
			flash.color[2] = b;
			return;
		}
	}

	void EffectsSystem::on_shot(const b2Vec2& muzzle, const b2Vec2& direction)
	{
		const float at[3] = { muzzle.x + kMuzzleAhead * direction.x, kGunHeight, -(muzzle.y + kMuzzleAhead * direction.y) };
		on_shot_at(at, direction);
	}

	void EffectsSystem::set_casings(const std::string& model, float scale, float life, size_t count)
	{
		if (!model.empty()) m_shell = model;
		m_casingScale = scale;
		m_casingLife = std::max(0.3f, life);
		m_casings.clear();
		m_nextCasing = 0;
		if (m_shell.empty() || !m_make) return;
		for (size_t i = 0; i < count; ++i)
		{
			Casing casing;
			casing.mesh = m_make(m_shell);
			if (!casing.mesh) break;
			casing.mesh->set_scale(0.0f);
			m_casings.push_back(std::move(casing));
		}
	}

	void EffectsSystem::on_shot_at(const float at[3], const b2Vec2& direction)
	{
		const b2Vec2 muzzle(at[0], -at[2]);
		const float gunHeight = at[1];
		m_shotHeight = std::clamp(gunHeight - (m_floor ? m_floor(muzzle) : 0.0f), 0.3f, 2.2f) + (m_floor ? m_floor(muzzle) : 0.0f);
		// The flash model is a flat plane whose flame trails toward -X: that side points the way the bullet flies
		const float yaw = std::atan2(direction.y, -direction.x);
		start_flash(muzzle.x, muzzle.y, gunHeight, yaw, kMuzzleScale * random(0.8f, 1.3f), kMuzzleLife, 1.0f, 1.0f, 1.0f);
		// ... and lights the floor around it, a bit ahead of the gun, never twice alike (it flickers with the shots)
		m_lights.push_back({ muzzle.x + 0.5f * direction.x, muzzle.y + 0.5f * direction.y, kMuzzleLightRadius * random(0.85f, 1.15f),
			0.0f, 0.0f, kMuzzleLightLife * random(0.8f, 1.3f), 1.0f, random(0.72f, 0.85f), 0.38f, random(0.28f, 0.4f), true });

		// A casing flies out to the right of the gun (a free one, else the oldest is taken again)
		if (!m_casings.empty())
		{
			Casing* free = nullptr;
			for (auto& casing : m_casings) if (casing.age < 0.0f) { free = &casing; break; }
			if (!free) free = &m_casings[m_nextCasing++ % m_casings.size()];
			Casing& casing = *free;
			const b2Vec2 side(direction.y, -direction.x);
			casing.age = 0.0f;
			casing.x = muzzle.x;
			casing.y = muzzle.y;
			casing.h = gunHeight;
			const float out = random(1.2f, 2.4f);
			casing.vx = side.x * out + direction.x * random(-0.3f, 0.5f);
			casing.vy = side.y * out + direction.y * random(-0.3f, 0.5f);
			casing.vh = random(1.5f, 2.8f);
			casing.spin = random(-25.0f, 25.0f);
			casing.turn = random(0.0f, 6.28f);
			casing.bounces = 0;
		}
	}

	void EffectsSystem::on_impact(const b2Vec2& at, const b2Vec2& direction, ImpactKind kind)
	{
		if (kind == ImpactKind::Enemy && !m_bloodShown)
		{
			return;   // no blood: a hit on a body shows nothing (its own flash says it was hit)
		}
		const bool blood = kind == ImpactKind::Enemy;
		start_flash(at.x - 0.05f * direction.x, at.y - 0.05f * direction.y, kGunHeight, random(0.0f, 6.28f),
			blood ? 0.5f : 0.32f, blood ? 0.2f : 0.11f, blood ? m_blood[0] * 2.0f : 1.0f, blood ? m_blood[1] * 2.0f : 0.85f, blood ? m_blood[2] * 2.0f : 0.5f);

		const int count = blood ? 7 : 6;
		for (int i = 0; i < count; ++i)
		{
			// Sparks bounce back from the wall; blood splashes on through the body
			const float spread = random(-1.0f, 1.0f);
			const b2Vec2 base = blood ? direction : -1.0f * direction;
			const b2Vec2 sideways(-base.y, base.x);
			const float speed = random(2.0f, blood ? 4.5f : 6.5f);
			Spark spark;
			spark.x = at.x;
			spark.y = at.y;
			spark.h = kGunHeight;
			spark.vx = (base.x + sideways.x * spread * 0.9f) * speed;
			spark.vy = (base.y + sideways.y * spread * 0.9f) * speed;
			spark.vh = random(0.5f, 3.0f);
			spark.age = 0.0f;
			spark.life = random(0.12f, blood ? 0.35f : 0.25f);
			spark.color = blood ? abgr(m_blood, 1.6f) : 0xff60e0ffu;
			m_sparks.push_back(spark);
		}
		// some of the blood lands behind the one hit
		if (blood && random(0.0f, 1.0f) < 0.45f)
		{
			const float out = random(0.4f, 1.4f);
			add_stain(shaped({ at.x + direction.x * out, at.y + direction.y * out, random(0.12f, 0.3f), 0.15f, 0.0f, kStainLife,
				m_blood[0], m_blood[1], m_blood[2], random(0.6f, 0.85f), false }, "splat"));
		}
	}

	void EffectsSystem::set_blood(bool shown, float red, float green, float blue)
	{
		m_bloodShown = shown;
		m_blood[0] = red;
		m_blood[1] = green;
		m_blood[2] = blue;
		if (!shown) m_stains.clear();
	}

	uint32_t EffectsSystem::abgr(const float rgb[3], float bright, float alpha)
	{
		auto channel = [](float v) { return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
		return (channel(alpha) << 24) | (channel(rgb[2] * bright) << 16) | (channel(rgb[1] * bright) << 8) | channel(rgb[0] * bright);
	}

	int EffectsSystem::sprite(const char* name, int* frames)
	{
		const auto found = m_sprites.find(name);
		if (found == m_sprites.end() || !m_graphics || !m_graphics->has_particle_sprites())
		{
			if (frames) *frames = 1;
			return 0;
		}
		if (frames)
		{
			*frames = std::max(1, found->second.second);
			return found->second.first;
		}
		const int variants = std::max(1, found->second.second);
		return found->second.first + std::min(variants - 1, static_cast<int>(random(0.0f, static_cast<float>(variants))));
	}

	EffectsSystem::Stain EffectsSystem::shaped(Stain stain, const char* name)
	{
		stain.sprite = sprite(name);
		stain.angle = random(0.0f, 6.2831853f);
		return stain;
	}

	void EffectsSystem::add_stain(const Stain& stain)
	{
		if (m_water && m_water(b2Vec2(stain.x, stain.y))) return;   // no blood or cracks on the water
		// over the most: the oldest start fading out (none disappears at once, a blink when there is a lot of blood); past
		// a margin more for the ones fading, the oldest goes
		if (m_stains.size() >= kMostStains)
		{
			for (Stain& old : m_stains)
			{
				if (old.light || old.life - old.age <= kStainFade) continue;
				old.life = old.age + kStainFade;
				break;
			}
		}
		if (m_stains.size() >= kMostStains + 60) m_stains.erase(m_stains.begin());
		m_stains.push_back(stain);
	}

	// Drops of blood thrown away from a blow: they fly and land as splats, the farther ones smaller
	void EffectsSystem::splatter(float x, float y, const b2Vec2& away, int drops, float reach, float size, const float rgb[3])
	{
		for (int i = 0; i < drops; ++i)
		{
			const float spread = random(-0.9f, 0.9f);
			const float c = std::cos(spread), s = std::sin(spread);
			const b2Vec2 way(away.x * c - away.y * s, away.x * s + away.y * c);
			const float out = random(0.3f, 1.0f) * reach;
			add_stain(shaped({ x + way.x * out, y + way.y * out, size * random(0.35f, 1.0f) * (1.2f - 0.6f * out / std::max(0.01f, reach)),
				random(0.05f, 0.2f), 0.0f, kStainLife, rgb[0] * random(0.8f, 1.1f), rgb[1] * random(0.8f, 1.1f), rgb[2], random(0.65f, 0.9f), false }, "splat"));
		}
	}

	void EffectsSystem::on_body_down(const b2Vec2& at, const b2Vec2& from, bool burst, bool big)
	{
		b2Vec2 away = at - from;
		if (away.Normalize() < 0.001f)
		{
			const float angle = random(0.0f, 6.2831853f);
			away = b2Vec2(std::cos(angle), std::sin(angle));
		}
		const float scale = big ? 1.5f : 1.0f;
		if (!m_bloodShown)
		{
			// no blood: a body that bursts goes up in a puff of dust
			if (burst) burst_debris(at.x, at.y, 0.8f * scale, 0);
			return;
		}
		const float* blood = m_blood;
		if (!burst)
		{
			// a pool spreading under the body
			add_stain(shaped({ at.x, at.y, random(0.55f, 0.8f), 1.2f, 0.0f, kStainLife, blood[0] * 0.85f, blood[1] * 0.85f, blood[2] * 0.85f, 0.85f, false }, "pool"));
			splatter(at.x, at.y, away, 3, 1.2f, 0.22f, blood);
			return;
		}
		// chunks thrown up and away, bouncing on the floor; a red mist; spatter on the floor
		const int chunks = static_cast<int>((big ? 22.0f : 14.0f));
		for (int i = 0; i < chunks; ++i)
		{
			const float spread = random(-1.3f, 1.3f);
			const float c = std::cos(spread), s = std::sin(spread);
			const float speed = random(1.5f, 5.0f) * scale;
			const float flesh = random(0.0f, 1.0f);
			add_puff({ at.x, at.y, random(0.6f, 1.3f), (away.x * c - away.y * s) * speed, (away.x * s + away.y * c) * speed,
				random(2.0f, 5.5f) * scale, 0.0f, random(1.6f, 2.6f), random(0.06f, 0.13f) * scale, 0.05f, 1.0f, 0.0f,
				blood[0] * (0.8f + 0.5f * flesh) + 0.1f * flesh, blood[1] * (0.8f + 0.5f * flesh) + 0.08f * flesh, blood[2] + 0.06f * flesh, kGravity, false, true });
		}
		for (int i = 0; i < 10; ++i)
		{
			const float angle = random(0.0f, 6.2831853f), out = random(0.2f, 1.2f);
			add_puff({ at.x, at.y, random(0.7f, 1.4f), std::cos(angle) * out + away.x, std::sin(angle) * out + away.y, random(0.2f, 0.8f),
				0.0f, random(0.4f, 0.7f), 0.25f * scale, (0.8f + 0.3f * random(0.0f, 1.0f)) * scale, 0.7f, 0.0f, blood[0] * 1.3f, blood[1] * 1.3f, blood[2] * 1.3f, 0.6f, false, false });
		}
		for (int i = 0; i < 18; ++i)
		{
			const float spread = random(-1.0f, 1.0f);
			const float speed = random(2.5f, 6.0f) * scale;
			const b2Vec2 side(-away.y, away.x);
			Spark spark;
			spark.x = at.x;
			spark.y = at.y;
			spark.h = random(0.8f, 1.3f);
			spark.vx = (away.x + side.x * spread) * speed;
			spark.vy = (away.y + side.y * spread) * speed;
			spark.vh = random(1.0f, 4.0f);
			spark.age = 0.0f;
			spark.life = random(0.2f, 0.45f);
			spark.color = abgr(blood, 1.4f);
			m_sparks.push_back(spark);
		}
		add_stain(shaped({ at.x, at.y, random(0.9f, 1.2f) * scale, 0.25f, 0.0f, kStainLife, blood[0], blood[1], blood[2], 0.9f, false }, "splat"));
		splatter(at.x, at.y, away, big ? 10 : 6, 2.6f * scale, 0.45f * scale, blood);
	}

	void EffectsSystem::on_climb_out(const b2Vec2& at, bool manhole)
	{
		// dirt or grit thrown up, and a cloud that hangs over the hole
		for (int i = 0; i < 12; ++i)
		{
			const float angle = random(0.0f, 6.2831853f), speed = random(0.8f, 2.5f);
			const float shade = manhole ? random(0.3f, 0.42f) : random(0.22f, 0.32f);
			add_puff({ at.x + std::cos(angle) * 0.25f, at.y + std::sin(angle) * 0.25f, 0.05f, std::cos(angle) * speed, std::sin(angle) * speed,
				random(2.0f, 4.0f), 0.0f, random(0.8f, 1.4f), random(0.04f, 0.08f), 0.03f, 1.0f, 0.0f,
				shade * (manhole ? 1.0f : 1.15f), shade, shade * (manhole ? 1.0f : 0.75f), kGravity, false, true });
		}
		for (int i = 0; i < 8; ++i)
		{
			const float angle = random(0.0f, 6.2831853f);
			const float shade = manhole ? 0.5f : 0.45f;
			add_puff({ at.x, at.y, 0.2f, std::cos(angle) * 0.7f, std::sin(angle) * 0.7f, random(0.3f, 0.7f), 0.0f, random(1.2f, 1.8f),
				0.3f, 0.9f, 0.5f, 0.0f, shade * 1.1f, shade, shade * 0.85f, 0.0f, false, false });
		}
		// the broken ground stays a while (not a manhole: its lid is the hole)
		if (!manhole) add_stain({ at.x, at.y, 0.65f, 0.4f, 0.0f, 12.0f, 0.16f, 0.12f, 0.08f, 0.7f, false });
	}

	void EffectsSystem::add_fountain(const b2Vec2& at, float radius, float height)
	{
		Emitter fountain{ Emitter::Kind::Fountain, at.x, at.y, radius, 1.0e9f };
		fountain.height = height;
		m_emitters.push_back(fountain);
	}

	void EffectsSystem::on_power_up(const b2Vec2& at, const float rgb[3])
	{
		// light spiraling up around the body
		for (int i = 0; i < 36; ++i)
		{
			const float angle = 6.2831853f * static_cast<float>(i) / 36.0f;
			const float out = random(0.4f, 0.7f);
			add_puff({ at.x + std::cos(angle) * out, at.y + std::sin(angle) * out, random(0.0f, 0.4f),
				-std::sin(angle) * 1.6f + std::cos(angle) * 0.4f, std::cos(angle) * 1.6f + std::sin(angle) * 0.4f, random(2.0f, 4.0f), 0.0f,
				random(0.6f, 1.0f), random(0.08f, 0.14f), 0.02f, 1.0f, 0.0f, rgb[0], rgb[1], rgb[2], -1.0f, true, false });
		}
		for (int i = 0; i < 24; ++i)
		{
			const float angle = random(0.0f, 6.2831853f), speed = random(3.0f, 7.0f);
			Spark spark;
			spark.x = at.x;
			spark.y = at.y;
			spark.h = random(0.3f, 1.6f);
			spark.vx = std::cos(angle) * speed;
			spark.vy = std::sin(angle) * speed;
			spark.vh = random(1.0f, 5.0f);
			spark.age = 0.0f;
			spark.life = random(0.25f, 0.5f);
			spark.color = abgr(rgb, 1.0f);
			m_sparks.push_back(spark);
		}
		// a ring of light runs out over the floor, and a flash at the feet
		m_lights.push_back({ at.x, at.y, 3.2f, 0.25f, 0.0f, 0.55f, rgb[0], rgb[1], rgb[2], 0.9f, true });
		m_lights.push_back({ at.x, at.y, 1.4f, 0.0f, 0.0f, 0.3f, 1.0f, 1.0f, 1.0f, 0.6f, true });
		start_flash(at.x, at.y, 1.0f, random(0.0f, 6.28f), 2.2f, 0.25f, rgb[0], rgb[1], rgb[2]);
	}

	void EffectsSystem::on_camera_flash(const b2Vec2& at, const b2Vec2& toward)
	{
		const float white[3] = { 1.0f, 1.0f, 0.95f };
		const b2Vec2 lens = at + 0.4f * toward;
		start_flash(lens.x, lens.y, 1.4f, std::atan2(toward.y, toward.x), 3.0f, 0.12f, white[0], white[1], white[2]);
		// the glow: on the body's spot and a stretch of floor ahead, gone in a blink
		m_lights.push_back({ lens.x, lens.y, 2.5f, 0.0f, 0.0f, 0.18f, white[0], white[1], white[2], 1.0f, true });
		const b2Vec2 ahead = at + 2.2f * toward;
		m_lights.push_back({ ahead.x, ahead.y, 2.0f, 0.0f, 0.0f, 0.18f, white[0], white[1], white[2], 0.7f, true });
		for (int i = 0; i < 14; ++i)
		{
			const float spread = random(-0.5f, 0.5f);
			const b2Vec2 dir(toward.x * std::cos(spread) - toward.y * std::sin(spread), toward.x * std::sin(spread) + toward.y * std::cos(spread));
			const float speed = random(4.0f, 8.0f);
			add_puff({ lens.x, lens.y, 1.4f, dir.x * speed, dir.y * speed, random(-0.5f, 0.5f), 0.0f, random(0.12f, 0.22f),
				random(0.15f, 0.25f), 0.05f, 1.0f, 0.0f, white[0], white[1], white[2], 0.0f, true, false });
		}
	}

	void EffectsSystem::on_splash(const b2Vec2& at, float height, float size)
	{
		const float water[3] = { 0.85f, 0.93f, 1.0f };
		for (int i = 0; i < 10; ++i)
		{
			const float angle = random(0.0f, 6.2831853f), out = random(0.6f, 1.6f) * size;
			add_puff({ at.x, at.y, height + 0.05f, std::cos(angle) * out, std::sin(angle) * out, random(2.0f, 3.5f) * size, 0.0f,
				random(0.4f, 0.7f), random(0.05f, 0.09f) * (0.5f + size), 0.03f, 0.9f, 0.0f, water[0], water[1], water[2], 6.0f, false, false });
		}
		// the ripple: a ring on the water spreading and fading (not a mark: lights are drawn on any floor)
		if (const int ring = sprite("ring"))
		{
			Stain wave{ at.x, at.y, 1.6f * size + 0.6f, 0.9f, 0.0f, 1.1f, water[0], water[1], water[2], 0.6f, true };
			wave.sprite = ring;
			wave.angle = random(0.0f, 6.2831853f);
			m_lights.push_back(wave);
		}
	}

	int EffectsSystem::stock_pieces(const std::vector<std::string>& files, int copies)
	{
		const int set = m_pieceSets++;
		// a copy of each model, then the next copy of each: the pieces of one body (taken one after another) are all
		// different (not three heads)
		for (int i = 0; i < copies; ++i)
		{
			for (const std::string& file : files)
			{
				Piece piece;
				piece.mesh = m_make ? m_make(file) : nullptr;
				if (!piece.mesh) continue;
				piece.mesh->set_scale(0.0f);
				piece.set = set;
				m_pieces.push_back(std::move(piece));
			}
		}
		return set;
	}

	void EffectsSystem::throw_pieces(const b2Vec2& at, const b2Vec2& from, const PieceThrow& how)
	{
		if (m_pieces.empty()) return;
		int count = how.count;
		b2Vec2 away = at - from;
		if (away.Normalize() < 1e-4f) away = b2Vec2(1.0f, 0.0f);
		// free ones, from a random place in the pool (so the same piece is not always first)
		const size_t start = static_cast<size_t>(random(0.0f, static_cast<float>(m_pieces.size())));
		for (size_t k = 0; k < m_pieces.size() && count > 0; ++k)
		{
			Piece& piece = m_pieces[(start + k) % m_pieces.size()];
			if (piece.age >= 0.0f || piece.set != how.set) continue;
			--count;
			const float angle = random(-0.5f * how.spread, 0.5f * how.spread);
			const b2Vec2 way(away.x * std::cos(angle) - away.y * std::sin(angle), away.x * std::sin(angle) + away.y * std::cos(angle));
			const float out = random(how.speed[0], how.speed[1]);
			piece.age = 0.0f;
			piece.life = 30.0f;   // (until it settles: then its 'lie')
			piece.lie = how.lie;
			piece.scale = how.scale;
			piece.gravity = how.gravity;
			piece.bounce = how.bounce;
			piece.tumble = how.tumble;
			piece.settled = false;
			piece.x = at.x;
			piece.y = at.y;
			piece.h = (m_floor ? m_floor(at) : 0.0f) + random(how.start[0], how.start[1]);
			piece.vx = way.x * out;
			piece.vy = way.y * out;
			piece.vh = random(how.up[0], how.up[1]);
			piece.spin = random(how.spin[0], how.spin[1]) * (random(0.0f, 1.0f) < 0.5f ? -1.0f : 1.0f);
			piece.sinking = false;
			piece.turn = random(0.0f, 6.28f);
			piece.tilt = random(0.0f, 6.28f);
		}
	}

	void EffectsSystem::on_firework(const b2Vec2& at, float height, const float rgb[3])
	{
		// stars thrown out every way (an even spread over the sphere), each a glowing dot that drifts down and dims
		for (int i = 0; i < 70; ++i)
		{
			const float up = random(-1.0f, 1.0f), angle = random(0.0f, 6.2831853f), flat = std::sqrt(std::max(0.0f, 1.0f - up * up));
			const float speed = random(4.5f, 6.0f);
			add_puff({ at.x, at.y, height, std::cos(angle) * flat * speed, std::sin(angle) * flat * speed, up * speed, 0.0f,
				random(1.1f, 1.7f), random(0.12f, 0.18f), 0.05f, 1.0f, 0.0f, rgb[0], rgb[1], rgb[2], 2.5f, true, false });
		}
		// and a few white streaks
		for (int i = 0; i < 24; ++i)
		{
			const float up = random(-1.0f, 1.0f), angle = random(0.0f, 6.2831853f), flat = std::sqrt(std::max(0.0f, 1.0f - up * up));
			const float speed = random(5.0f, 8.0f);
			Spark spark;
			spark.x = at.x;
			spark.y = at.y;
			spark.h = height;
			spark.vx = std::cos(angle) * flat * speed;
			spark.vy = std::sin(angle) * flat * speed;
			spark.vh = up * speed;
			spark.age = 0.0f;
			spark.life = random(0.4f, 0.8f);
			const float white[3] = { 1.0f, 0.95f, 0.85f };
			spark.color = abgr(white, 1.0f);
			m_sparks.push_back(spark);
		}
		m_lights.push_back({ at.x, at.y, 4.0f, 0.0f, 0.0f, 0.7f, rgb[0], rgb[1], rgb[2], 0.3f, true });   // (stronger, ten of them whited the screen out)
	}

	void EffectsSystem::aura(const b2Vec2& at, const float rgb[3], float dt)
	{
		m_auraClock += dt;
		const float pulse = 0.5f + 0.5f * std::sin(m_auraClock * 6.0f);
		m_graphics->draw_ground_spot(Pakal::tmath::vector3df(at.x, kStainHeight, -at.y), 1.0f + 0.15f * pulse,
			abgr(rgb, 1.0f, 0.22f + 0.12f * pulse), true);
		// sparks rising from the body
		m_auraDue += dt * 14.0f;
		for (; m_auraDue >= 1.0f; m_auraDue -= 1.0f)
		{
			const float angle = random(0.0f, 6.2831853f), out = random(0.15f, 0.45f);
			add_puff({ at.x + std::cos(angle) * out, at.y + std::sin(angle) * out, random(0.1f, 1.2f), 0.0f, 0.0f, random(0.8f, 1.6f), 0.0f,
				random(0.5f, 0.8f), random(0.04f, 0.07f), 0.01f, 0.9f, 0.0f, rgb[0], rgb[1], rgb[2], 0.0f, true, false });
		}
	}

	void EffectsSystem::on_player_hit(const b2Vec2& at)
	{
		if (!m_bloodShown) return;
		for (int i = 0; i < 9; ++i)
		{
			const float angle = random(0.0f, 6.2831853f), speed = random(1.5f, 3.5f);
			Spark spark;
			spark.x = at.x;
			spark.y = at.y;
			spark.h = 1.2f;
			spark.vx = std::cos(angle) * speed;
			spark.vy = std::sin(angle) * speed;
			spark.vh = random(0.5f, 2.5f);
			spark.age = 0.0f;
			spark.life = random(0.15f, 0.35f);
			spark.color = 0xff2020c8u;
			m_sparks.push_back(spark);
		}
		add_stain({ at.x + random(-0.3f, 0.3f), at.y + random(-0.3f, 0.3f), random(0.15f, 0.3f), 0.1f, 0.0f, kStainLife,
			m_settings.blood[0], m_settings.blood[1], m_settings.blood[2], 0.75f, false });
	}

	void EffectsSystem::add_puff(const Puff& puff)
	{
		// the GPU moves and draws it when it can (nothing waits for it to land): handed over once, in render space
		if (m_graphics && m_graphics->gpu_particles() && puff.sprite == 0)
		{
			Pakal::GpuParticles::Particle p;
			p.position[0] = puff.x; p.position[1] = puff.h; p.position[2] = -puff.y;
			p.velocity[0] = puff.vx; p.velocity[1] = puff.vh; p.velocity[2] = -puff.vy;
			p.life = puff.life - puff.age;
			p.color[0] = puff.red; p.color[1] = puff.green; p.color[2] = puff.blue;
			p.gravity = puff.gravity;
			p.radius0 = puff.r0; p.radius1 = puff.r1;
			p.alpha0 = puff.a0; p.alpha1 = puff.a1;
			p.additive = puff.additive;
			p.bounce = puff.bounce;
			m_graphics->emit_gpu_particle(p);
			return;
		}
		if (m_puffs.size() < kMostPuffs) m_puffs.push_back(puff);
	}

	// Chunks flying off a broken thing, and a cloud of dust
	void EffectsSystem::burst_debris(float x, float y, float size, int count)
	{
		for (int i = 0; i < count; ++i)
		{
			const float angle = random(0.0f, 6.2831853f), speed = random(1.5f, 4.5f);
			const float shade = random(0.22f, 0.45f);
			add_puff({ x + random(-0.3f, 0.3f) * size, y + random(-0.3f, 0.3f) * size, random(0.2f, 0.8f),
				std::cos(angle) * speed, std::sin(angle) * speed, random(2.5f, 6.0f), 0.0f, random(1.0f, 1.8f),
				random(0.05f, 0.11f), 0.05f, 1.0f, 0.0f, shade, shade * 0.95f, shade * 0.9f, kGravity, false, true });
		}
		for (int i = 0; i < 6; ++i)
		{
			const float angle = random(0.0f, 6.2831853f);
			add_puff({ x, y, 0.4f, std::cos(angle) * 0.8f, std::sin(angle) * 0.8f, 0.4f, 0.0f, random(0.8f, 1.3f),
				0.3f, 0.5f + 0.35f * size, 0.45f, 0.0f, 0.62f, 0.6f, 0.56f, 0.0f, false, false });
		}
	}

	void EffectsSystem::on_prop_effect(const b2Vec2& at, const std::string& effect, float size)
	{
		// the first of the game's names that fits it
		const PropEffectName* name = nullptr;
		for (const auto& candidate : m_settings.propEffects)
		{
			if (candidate.whole ? effect == candidate.word : effect.find(candidate.word) != std::string::npos) { name = &candidate; break; }
		}
		if (!name) return;
		if (name->effect == PropEffect::Water)
		{
			m_emitters.push_back({ Emitter::Kind::Water, at.x, at.y, size, m_settings.waterSeconds });
		}
		else if (name->effect == PropEffect::Fire)
		{
			m_emitters.push_back({ Emitter::Kind::Fire, at.x, at.y, size, m_settings.fireSeconds });
		}
		else if (name->effect == PropEffect::Glass)
		{
			// a window breaks: pale shards
			for (int i = 0; i < 26; ++i)
			{
				const float angle = random(0.0f, 6.2831853f), speed = random(1.5f, 3.5f);
				add_puff({ at.x, at.y, random(0.9f, 1.4f), std::cos(angle) * speed, std::sin(angle) * speed, random(1.0f, 3.0f),
					0.0f, random(0.7f, 1.1f), 0.045f, 0.03f, 0.95f, 0.2f, 0.75f, 0.9f, 1.0f, kGravity, false, true });
			}
		}
		else
		{
			burst_debris(at.x, at.y, size, 8 + static_cast<int>(size * 5.0f));
		}
	}

	void EffectsSystem::set_fire(int key, const b2Vec2& at, float size, bool on)
	{
		for (auto& emitter : m_emitters)
		{
			if (emitter.kind != Emitter::Kind::Fire || emitter.key != key) continue;
			emitter.secondsLeft = on ? 1e9f : std::min(emitter.secondsLeft, 4.0f);
			return;
		}
		if (on)
		{
			Emitter fire{ Emitter::Kind::Fire, at.x, at.y, size, 1e9f };
			fire.key = key;
			m_emitters.push_back(fire);
		}
	}

	void EffectsSystem::update_emitters(float dt)
	{
		for (auto& emitter : m_emitters)
		{
			emitter.secondsLeft -= dt;
			const float fading = std::min(1.0f, emitter.secondsLeft / 5.0f); // it dies down over its last seconds
			if (emitter.kind == Emitter::Kind::Fountain)
			{
				// jets rising from the top and arching back into the basin, and a little spray where they land
				emitter.waterDue += dt * m_settings.fountainRate;
				for (; emitter.waterDue >= 1.0f; emitter.waterDue -= 1.0f)
				{
					const float angle = random(0.0f, 6.2831853f), out = random(0.35f, 0.6f) * emitter.size;
					const float rise = random(1.6f, 2.4f);
					const float flight = (rise + std::sqrt(rise * rise + 2.0f * kGravity * emitter.height * 0.4f)) / kGravity;
					add_puff({ emitter.x, emitter.y, emitter.height, std::cos(angle) * out / flight, std::sin(angle) * out / flight, rise, 0.0f,
						flight + 0.2f, 0.05f, 0.11f, 0.7f, 0.15f, 0.78f, 0.9f, 1.0f, kGravity, false, false });
				}
				continue;
			}
			if (emitter.kind == Emitter::Kind::Water)
			{
				// a jet up from what broke, falling back as spray
				emitter.waterDue += dt * m_settings.waterRate * fading;
				for (; emitter.waterDue >= 1.0f; emitter.waterDue -= 1.0f)
				{
					const float angle = random(0.0f, 6.2831853f), out = random(0.2f, 0.9f);
					add_puff({ emitter.x + random(-0.05f, 0.05f), emitter.y + random(-0.05f, 0.05f), 0.55f,
						std::cos(angle) * out, std::sin(angle) * out, random(5.5f, 7.5f) * (0.6f + 0.4f * fading), 0.0f,
						random(1.1f, 1.5f), 0.1f, 0.4f, 0.85f, 0.0f, 0.8f, 0.92f, 1.0f, kGravity, false, false });
				}
			}
			else
			{
				// flames over the wreck and a column of smoke
				const float area = std::max(1.0f, emitter.size / 4.0f);
				emitter.fireDue += dt * m_settings.fireRate * area * fading;
				emitter.smokeDue += dt * m_settings.smokeRate * area * fading;
				const float spread = emitter.size * 0.3f;
				// with the flipbook, a quarter of the flames, bigger: each one is a whole tongue of fire, not a spark of it
				int frames = 1;
				const int flames = sprite("fire", &frames);
				if (flames) emitter.fireDue -= 0.75f * dt * m_settings.fireRate * area * fading;
				for (; emitter.fireDue >= 1.0f; emitter.fireDue -= 1.0f)
				{
					Puff flame{ emitter.x + random(-spread, spread), emitter.y + random(-spread, spread), random(0.6f, 1.1f),
						random(-0.2f, 0.2f), random(-0.2f, 0.2f), random(1.4f, 2.6f), 0.0f, random(0.5f, 0.9f),
						random(0.45f, 0.7f), 0.08f, 1.0f, 0.0f, 1.0f, random(0.35f, 0.6f), 0.1f, -1.5f, true, false };
					if (flames)
					{
						flame.h = random(0.3f, 0.6f);
						flame.vh = random(0.6f, 1.2f);
						flame.life = random(0.6f, 0.9f);
						flame.r0 = random(0.55f, 0.85f) * std::min(1.6f, std::max(0.8f, emitter.size / 3.0f));
						flame.r1 = flame.r0 * 0.6f;
						flame.a0 = 0.95f;
						flame.a1 = 0.2f;
						flame.green = random(0.45f, 0.65f);
						flame.blue = 0.2f;
						flame.sprite = flames;
						flame.frames = frames;
						flame.angle = random(-0.25f, 0.25f);
					}
					add_puff(flame);
				}
				for (; emitter.smokeDue >= 1.0f; emitter.smokeDue -= 1.0f)
				{
					const float shade = random(0.22f, 0.34f);
					Puff cloud{ emitter.x + random(-spread, spread), emitter.y + random(-spread, spread), 1.8f,
						0.35f + random(-0.15f, 0.15f), 0.2f + random(-0.15f, 0.15f), random(1.4f, 2.0f), 0.0f, random(3.0f, 4.0f),
						0.5f, 2.0f, 0.65f, 0.0f, shade, shade, shade * 0.95f, -0.1f, false, false };
					cloud.sprite = sprite("smoke");
					cloud.angle = random(0.0f, 6.2831853f);
					cloud.spin = random(-0.6f, 0.6f);
					add_puff(cloud);
				}
			}
		}
		m_emitters.erase(std::remove_if(m_emitters.begin(), m_emitters.end(), [](const Emitter& e) { return e.secondsLeft <= 0.0f; }), m_emitters.end());
	}

	void EffectsSystem::on_shockwave(const b2Vec2& at, float radius)
	{
		const int count = 30;
		for (int i = 0; i < count; ++i)
		{
			const float angle = 6.2831853f * (static_cast<float>(i) + random(0.0f, 0.6f)) / count;
			const float speed = radius / 0.45f * random(0.8f, 1.1f);
			const float shade = random(0.5f, 0.62f);
			add_puff({ at.x + std::cos(angle) * 0.3f, at.y + std::sin(angle) * 0.3f, 0.15f, std::cos(angle) * speed, std::sin(angle) * speed,
				random(0.2f, 0.6f), 0.0f, random(0.5f, 0.7f), 0.3f, 0.9f + 0.1f * radius, 0.55f, 0.0f, shade, shade * 0.92f, shade * 0.8f,
				0.0f, false, false });
		}
		start_flash(at.x, at.y, 0.15f, random(0.0f, 6.28f), radius * 0.9f, 0.18f, 1.0f, 0.95f, 0.8f);
	}

	void EffectsSystem::on_blast(const b2Vec2& at, float radius)
	{
		// a ball of fire and the smoke it leaves
		int frames = 1;
		const int flames = sprite("fire", &frames);
		for (int i = 0; i < 16; ++i)
		{
			const float angle = random(0.0f, 6.2831853f), out = random(0.5f, 2.5f) * std::max(0.5f, radius / 4.0f);
			Puff ball{ at.x, at.y, random(0.4f, 1.2f), std::cos(angle) * out, std::sin(angle) * out, random(1.0f, 3.0f), 0.0f,
				random(0.35f, 0.6f), random(0.5f, 0.9f) * std::max(0.6f, radius / 3.0f), 0.2f, 1.0f, 0.0f, 1.0f, random(0.45f, 0.7f), 0.15f,
				-2.0f, true, false };
			if (flames)
			{
				ball.sprite = flames;
				ball.frames = frames;
				ball.r0 *= 1.4f;
				ball.r1 = ball.r0 * 0.7f;
				ball.angle = random(-0.4f, 0.4f);
				ball.life = random(0.5f, 0.8f);
			}
			add_puff(ball);
		}
		for (int i = 0; i < 10; ++i)
		{
			const float angle = random(0.0f, 6.2831853f), out = random(0.3f, 1.5f);
			const float shade = random(0.15f, 0.25f);
			Puff cloud{ at.x, at.y, random(0.8f, 1.6f), std::cos(angle) * out, std::sin(angle) * out, random(0.8f, 1.6f), 0.0f,
				random(2.0f, 3.0f), 0.6f, 1.6f + 0.2f * radius, 0.6f, 0.0f, shade, shade, shade, -0.2f, false, false };
			cloud.sprite = sprite("smoke");
			cloud.angle = random(0.0f, 6.2831853f);
			cloud.spin = random(-0.8f, 0.8f);
			add_puff(cloud);
		}
		if (const int ring = sprite("ring"))
		{
			// a ring of heat running out over the floor
			Stain wave{ at.x, at.y, radius * 1.3f, 0.3f, 0.0f, 0.45f, 1.0f, 0.75f, 0.4f, 0.9f, true };
			wave.sprite = ring;
			wave.angle = random(0.0f, 6.2831853f);
			m_lights.push_back(wave);
		}
		if (radius >= 2.0f)
		{
			// a big one cracks the floor
			Stain crack{ at.x, at.y, radius * 0.9f, 0.05f, 0.0f, kStainLife, 1.0f, 1.0f, 1.0f, 0.85f, false };
			crack.sprite = sprite("crack");
			crack.angle = random(0.0f, 6.2831853f);
			if (crack.sprite) add_stain(crack);
		}
		start_flash(at.x, at.y, 0.6f, random(0.0f, 6.28f), radius * 1.6f, 0.45f, 1.0f, 0.75f, 0.45f);
		start_flash(at.x, at.y, 1.2f, random(0.0f, 6.28f), radius * 1.0f, 0.3f, 1.0f, 0.95f, 0.8f);
		for (int i = 0; i < 26; ++i)
		{
			const float angle = random(0.0f, 6.2831853f);
			const float speed = random(3.0f, 9.0f);
			Spark spark;
			spark.x = at.x;
			spark.y = at.y;
			spark.h = 0.5f;
			spark.vx = std::cos(angle) * speed;
			spark.vy = std::sin(angle) * speed;
			spark.vh = random(2.0f, 8.0f);
			spark.age = 0.0f;
			spark.life = random(0.3f, 0.7f);
			spark.color = 0xff40a0ffu;
			m_sparks.push_back(spark);
		}
	}

	void EffectsSystem::update(float dt, const std::vector<Bullet>& bullets)
	{
		// the GPU's particles move by the game's time: they stop with it (a pause)
		if (m_graphics) m_graphics->advance_gpu_particles(dt);
		// Bullets: the first N take a model each, the others stay hidden
		for (size_t i = 0; i < m_bullets.size(); ++i)
		{
			Pakal::MeshComponent3D_Bgfx& mesh = *m_bullets[i];
			if (i < bullets.size())
			{
				const Bullet& bullet = bullets[i];
				// the player's at the height its gun's mouth fired from (a rifle at the shoulder, a minigun at the hip)
				mesh.set_position(bullet.position.x, bullet.fromEnemy ? kGunHeight : m_shotHeight, -bullet.position.y);
				mesh.set_rotation(0.0f, actor_yaw(bullet.direction.x, -bullet.direction.y), 0.0f);
				mesh.set_scale(1.0f);
			}
			else
			{
				mesh.set_scale(0.0f);
			}
		}

		for (auto& flash : m_flashes)
		{
			if (flash.age < 0.0f)
			{
				continue;
			}
			flash.age += dt;
			if (flash.age >= flash.life)
			{
				flash.age = -1.0f;
				flash.mesh->set_scale(0.0f);
				continue;
			}
			const float fade = 1.0f - flash.age / flash.life;
			flash.mesh->set_position(flash.x, flash.height, -flash.y);
			flash.mesh->set_rotation(0.0f, flash.yaw, 0.0f);
			flash.mesh->set_scale(flash.scale * (1.0f + 0.25f * (1.0f - fade)));
			flash.mesh->set_color(flash.color[0] * fade, flash.color[1] * fade, flash.color[2] * fade);
		}

		for (auto& casing : m_casings)
		{
			if (casing.age < 0.0f)
			{
				continue;
			}
			casing.age += dt;
			if (casing.age >= m_casingLife)
			{
				casing.age = -1.0f;
				casing.mesh->set_scale(0.0f);
				continue;
			}
			casing.vh -= kGravity * dt;
			casing.h += casing.vh * dt;
			const float ground = (m_floor ? m_floor(b2Vec2(casing.x, casing.y)) : 0.0f) + 0.01f;
			if (casing.h <= ground)
			{
				casing.h = ground;
				if (casing.bounces < 2 && casing.vh < -1.0f)
				{
					// it rings off the floor: up again, slower, a new spin
					casing.vh = -casing.vh * 0.4f;
					casing.vx *= 0.6f;
					casing.vy *= 0.6f;
					casing.spin = random(-20.0f, 20.0f);
					++casing.bounces;
				}
				else // lies and slides to a stop
				{
					casing.vh = 0.0f;
					const float keep = std::pow(0.02f, dt);
					casing.vx *= keep;
					casing.vy *= keep;
					casing.spin *= keep;
				}
			}
			casing.x += casing.vx * dt;
			casing.y += casing.vy * dt;
			casing.turn += casing.spin * dt;
			const float shrink = casing.age > m_casingLife - 0.25f ? (m_casingLife - casing.age) / 0.25f : 1.0f;
			casing.mesh->set_position(casing.x, casing.h, -casing.y);
			// in the air it tumbles end over end; on the floor it lies on its side, turned where it stopped
			const bool lying = casing.vh == 0.0f && casing.h <= (m_floor ? m_floor(b2Vec2(casing.x, casing.y)) : 0.0f) + 0.011f;
			casing.mesh->set_rotation(lying ? 0.0f : casing.turn, casing.turn * 0.5f, lying ? 0.0f : 1.2f);
			casing.mesh->set_scale(m_casingScale * shrink);
		}

		// the pieces thrown: in the air they tumble; they bounce once or twice and lie, then shrink away
		for (auto& piece : m_pieces)
		{
			if (piece.age < 0.0f) continue;
			piece.age += dt;
			if (piece.age >= piece.life)
			{
				piece.age = -1.0f;
				piece.mesh->set_scale(0.0f);
				continue;
			}
			const float ground = (m_floor ? m_floor(b2Vec2(piece.x, piece.y)) : 0.0f) + 0.08f;
			if (piece.sinking)
			{
				// in the water: it goes down slowly, drifting, and is gone
				piece.h -= 0.35f * dt;
				piece.x += piece.vx * dt;
				piece.y += piece.vy * dt;
				piece.vx *= 0.97f;
				piece.vy *= 0.97f;
				piece.life = std::min(piece.life, piece.age + std::max(0.0f, (piece.h - (ground - 0.6f)) / 0.35f));
				piece.mesh->set_position(piece.x, piece.h, -piece.y);
				continue;
			}
			if (piece.settled)
			{
				const float shrink = piece.age > piece.life - 0.4f ? (piece.life - piece.age) / 0.4f : 1.0f;
				piece.mesh->set_scale(piece.scale * shrink);
				continue;
			}
			piece.vh -= piece.gravity * dt;
			piece.h += piece.vh * dt;
			if (piece.h <= ground && m_water && m_water(b2Vec2(piece.x, piece.y)))
			{
				piece.sinking = true;
				piece.h = ground;
				on_splash(b2Vec2(piece.x, piece.y), ground, 0.5f);
				piece.vx *= 0.3f;
				piece.vy *= 0.3f;
				piece.vh = 0.0f;
				continue;
			}
			if (piece.h <= ground)
			{
				// it bounces, keeping 'bounce' of its speed, until a bounce would not lift it: then it lies
				piece.h = ground;
				if (-piece.vh * piece.bounce > 1.0f)
				{
					piece.vh = -piece.vh * piece.bounce;
					piece.vx *= piece.bounce;
					piece.vy *= piece.bounce;
					piece.spin *= piece.bounce;
				}
				else
				{
					piece.settled = true;
					piece.life = piece.age + piece.lie + 0.4f;
				}
			}
			piece.x += piece.vx * dt;
			piece.y += piece.vy * dt;
			piece.turn += piece.spin * dt;
			const float shrink = piece.age > piece.life - 0.4f ? (piece.life - piece.age) / 0.4f : 1.0f;
			piece.mesh->set_position(piece.x, piece.h, -piece.y);
			if (piece.tumble) piece.mesh->set_rotation(piece.tilt + piece.turn * 0.3f, piece.turn, piece.tilt);
			else piece.mesh->set_rotation(piece.tilt, piece.turn, 0.5f * piece.tilt);   // (the original's: about the vertical)
			piece.mesh->set_scale(piece.scale * shrink);
		}

		for (auto& spark : m_sparks)
		{
			spark.age += dt;
			const float px = spark.x, py = spark.y, ph = spark.h;
			spark.vh -= kSparkGravity * dt;
			spark.x += spark.vx * dt;
			spark.y += spark.vy * dt;
			spark.h = std::max(0.0f, spark.h + spark.vh * dt);
			if (spark.age < spark.life)
			{
				m_graphics->draw_line3d(Pakal::tmath::vector3df(px, ph, -py), Pakal::tmath::vector3df(spark.x, spark.h, -spark.y), spark.color, false);
			}
		}
		m_sparks.erase(std::remove_if(m_sparks.begin(), m_sparks.end(), [](const Spark& spark) { return spark.age >= spark.life; }), m_sparks.end());

		update_emitters(dt);
		for (auto& puff : m_puffs)
		{
			puff.age += dt;
			puff.vh -= puff.gravity * dt;
			puff.x += puff.vx * dt;
			puff.y += puff.vy * dt;
			puff.h += puff.vh * dt;
			if (puff.h <= 0.02f)
			{
				// on the floor: a chunk bounces and slides, a drop or a puff stays there spreading out
				puff.h = 0.02f;
				puff.vh = puff.bounce && puff.vh < -1.0f ? -puff.vh * 0.35f : 0.0f;
				puff.vx *= puff.bounce ? 0.7f : 0.9f;
				puff.vy *= puff.bounce ? 0.7f : 0.9f;
			}
			const float t = std::min(1.0f, puff.age / puff.life);
			const float radius = puff.r0 + (puff.r1 - puff.r0) * t;
			const float alpha = std::clamp(puff.a0 + (puff.a1 - puff.a0) * t, 0.0f, 1.0f);
			auto channel = [](float v) { return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
			const uint32_t abgr = (channel(alpha) << 24) | (channel(puff.blue) << 16) | (channel(puff.green) << 8) | channel(puff.red);
			const Pakal::tmath::vector3df at(puff.x, puff.h + radius * 0.5f, -puff.y);
			if (puff.sprite)
			{
				const int frame = std::min(puff.frames - 1, static_cast<int>(t * static_cast<float>(puff.frames)));
				m_graphics->draw_sprite_particle(at, radius, abgr, puff.additive, static_cast<uint16_t>(puff.sprite + frame),
					puff.angle + puff.spin * puff.age);
			}
			else
			{
				m_graphics->draw_particle(at, radius, abgr, puff.additive);
			}
		}
		m_puffs.erase(std::remove_if(m_puffs.begin(), m_puffs.end(), [](const Puff& puff) { return puff.age >= puff.life; }), m_puffs.end());

		// What lies on the floor: blood spreading to its size and fading at the end, the muzzle's light dying out
		auto draw_marks = [this, dt](std::vector<Stain>& marks)
		{
			for (auto& mark : marks)
			{
				mark.age += dt;
				const float spread = mark.grow > 0.0f ? std::sqrt(std::min(1.0f, mark.age / mark.grow)) : 1.0f;
				const float fade = mark.light ? 1.0f - mark.age / mark.life : std::min(1.0f, (mark.life - mark.age) / kStainFade);
				const float alpha = std::clamp(mark.alpha * fade, 0.0f, 1.0f);
				if (alpha <= 0.0f) continue;
				auto channel = [](float v) { return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
				const uint32_t abgr = (channel(alpha) << 24) | (channel(mark.blue) << 16) | (channel(mark.green) << 8) | channel(mark.red);
				const Pakal::tmath::vector3df at(mark.x, kStainHeight, -mark.y);
				if (mark.sprite)
				{
					m_graphics->draw_sprite_spot(at, mark.radius * std::max(0.15f, spread), abgr, mark.light, static_cast<uint16_t>(mark.sprite), mark.angle);
				}
				else
				{
					m_graphics->draw_ground_spot(at, mark.radius * std::max(0.15f, spread), abgr, mark.light);
				}
			}
			marks.erase(std::remove_if(marks.begin(), marks.end(), [](const Stain& mark) { return mark.age >= mark.life; }), marks.end());
		};
		draw_marks(m_stains);
		draw_marks(m_lights);
	}
}
