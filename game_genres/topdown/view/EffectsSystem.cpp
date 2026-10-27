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
		const float kWaterSeconds = 60.0f;      // a broken hydrant gushes this long
		const float kFireSeconds = 40.0f;       // a burnt car burns this long
		const float kWaterRate = 90.0f;         // particles per second
		const float kFireRate = 75.0f;          // ... per car about 4 m wide
		const float kSmokeRate = 10.0f;

		bool has(const std::string& text, const char* word) { return text.find(word) != std::string::npos; }
	}

	EffectsSystem::EffectsSystem(Pakal::BgfxGraphicsSystem* graphics, const MeshFactory& make, const std::string& bullet,
		const std::string& muzzle, const std::string& shell, uint32_t seed) :
		m_graphics(graphics),
		m_random(seed)
	{
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
		// The flash model is a flat plane whose flame trails toward -X: that side points the way the bullet flies
		const float yaw = std::atan2(direction.y, -direction.x);
		start_flash(muzzle.x + kMuzzleAhead * direction.x, muzzle.y + kMuzzleAhead * direction.y, kGunHeight, yaw,
			kMuzzleScale * random(0.8f, 1.3f), kMuzzleLife, 1.0f, 1.0f, 1.0f);

		// A casing flies out to the right of the gun
		for (auto& casing : m_casings)
		{
			if (casing.age >= 0.0f)
			{
				continue;
			}
			const b2Vec2 side(direction.y, -direction.x);
			casing.age = 0.0f;
			casing.x = muzzle.x;
			casing.y = muzzle.y;
			casing.h = kGunHeight;
			const float out = random(1.2f, 2.4f);
			casing.vx = side.x * out + direction.x * random(-0.3f, 0.5f);
			casing.vy = side.y * out + direction.y * random(-0.3f, 0.5f);
			casing.vh = random(1.5f, 2.8f);
			casing.spin = random(-25.0f, 25.0f);
			casing.turn = random(0.0f, 6.28f);
			break;
		}
	}

	void EffectsSystem::on_impact(const b2Vec2& at, const b2Vec2& direction, ImpactKind kind)
	{
		const bool blood = kind == ImpactKind::Enemy;
		start_flash(at.x - 0.05f * direction.x, at.y - 0.05f * direction.y, kGunHeight, random(0.0f, 6.28f),
			blood ? 0.5f : 0.32f, blood ? 0.2f : 0.11f, blood ? 0.9f : 1.0f, blood ? 0.12f : 0.85f, blood ? 0.1f : 0.5f);

		const int count = blood ? 7 : 6;
		for (int i = 0; i < count; ++i)
		{
			// Sparks bounce back from the wall; blood splashes on through the zombie
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
			spark.color = blood ? 0xff2020c0u : 0xff60e0ffu; // ABGR
			m_sparks.push_back(spark);
		}
	}

	void EffectsSystem::add_puff(const Puff& puff)
	{
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
		if (has(effect, "Water"))
		{
			m_emitters.push_back({ Emitter::Kind::Water, at.x, at.y, size, kWaterSeconds });
		}
		else if (has(effect, "Fire"))
		{
			m_emitters.push_back({ Emitter::Kind::Fire, at.x, at.y, size, kFireSeconds });
		}
		else if (has(effect, "Glass"))
		{
			// a window breaks: pale shards
			for (int i = 0; i < 26; ++i)
			{
				const float angle = random(0.0f, 6.2831853f), speed = random(1.5f, 3.5f);
				add_puff({ at.x, at.y, random(0.9f, 1.4f), std::cos(angle) * speed, std::sin(angle) * speed, random(1.0f, 3.0f),
					0.0f, random(0.7f, 1.1f), 0.045f, 0.03f, 0.95f, 0.2f, 0.75f, 0.9f, 1.0f, kGravity, false, true });
			}
		}
		else if (effect == "Break")
		{
			burst_debris(at.x, at.y, size, 8 + static_cast<int>(size * 5.0f));
		}
	}

	void EffectsSystem::update_emitters(float dt)
	{
		for (auto& emitter : m_emitters)
		{
			emitter.secondsLeft -= dt;
			const float fading = std::min(1.0f, emitter.secondsLeft / 5.0f); // it dies down over its last seconds
			if (emitter.kind == Emitter::Kind::Water)
			{
				// a jet up from the broken hydrant, falling back as spray
				emitter.waterDue += dt * kWaterRate * fading;
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
				emitter.fireDue += dt * kFireRate * area * fading;
				emitter.smokeDue += dt * kSmokeRate * area * fading;
				const float spread = emitter.size * 0.3f;
				for (; emitter.fireDue >= 1.0f; emitter.fireDue -= 1.0f)
				{
					add_puff({ emitter.x + random(-spread, spread), emitter.y + random(-spread, spread), random(0.6f, 1.1f),
						random(-0.2f, 0.2f), random(-0.2f, 0.2f), random(1.4f, 2.6f), 0.0f, random(0.5f, 0.9f),
						random(0.45f, 0.7f), 0.08f, 1.0f, 0.0f, 1.0f, random(0.35f, 0.6f), 0.1f, -1.5f, true, false });
				}
				for (; emitter.smokeDue >= 1.0f; emitter.smokeDue -= 1.0f)
				{
					const float shade = random(0.22f, 0.34f);
					add_puff({ emitter.x + random(-spread, spread), emitter.y + random(-spread, spread), 1.8f,
						0.35f + random(-0.15f, 0.15f), 0.2f + random(-0.15f, 0.15f), random(1.4f, 2.0f), 0.0f, random(3.0f, 4.0f),
						0.5f, 2.0f, 0.65f, 0.0f, shade, shade, shade * 0.95f, -0.1f, false, false });
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
		for (int i = 0; i < 16; ++i)
		{
			const float angle = random(0.0f, 6.2831853f), out = random(0.5f, 2.5f) * std::max(0.5f, radius / 4.0f);
			add_puff({ at.x, at.y, random(0.4f, 1.2f), std::cos(angle) * out, std::sin(angle) * out, random(1.0f, 3.0f), 0.0f,
				random(0.35f, 0.6f), random(0.5f, 0.9f) * std::max(0.6f, radius / 3.0f), 0.2f, 1.0f, 0.0f, 1.0f, random(0.45f, 0.7f), 0.15f,
				-2.0f, true, false });
		}
		for (int i = 0; i < 10; ++i)
		{
			const float angle = random(0.0f, 6.2831853f), out = random(0.3f, 1.5f);
			const float shade = random(0.15f, 0.25f);
			add_puff({ at.x, at.y, random(0.8f, 1.6f), std::cos(angle) * out, std::sin(angle) * out, random(0.8f, 1.6f), 0.0f,
				random(2.0f, 3.0f), 0.6f, 1.6f + 0.2f * radius, 0.6f, 0.0f, shade, shade, shade, -0.2f, false, false });
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
		// Bullets: the first N take a model each, the others stay hidden
		for (size_t i = 0; i < m_bullets.size(); ++i)
		{
			Pakal::MeshComponent3D_Bgfx& mesh = *m_bullets[i];
			if (i < bullets.size())
			{
				const Bullet& bullet = bullets[i];
				mesh.set_position(bullet.position.x, kGunHeight, -bullet.position.y);
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
			if (casing.age >= kCasingLife)
			{
				casing.age = -1.0f;
				casing.mesh->set_scale(0.0f);
				continue;
			}
			casing.vh -= kGravity * dt;
			casing.h += casing.vh * dt;
			if (casing.h <= 0.03f) // lands and slides to a stop
			{
				casing.h = 0.03f;
				casing.vh = 0.0f;
				casing.vx *= 0.8f;
				casing.vy *= 0.8f;
				casing.spin *= 0.8f;
			}
			casing.x += casing.vx * dt;
			casing.y += casing.vy * dt;
			casing.turn += casing.spin * dt;
			const float shrink = casing.age > kCasingLife - 0.25f ? (kCasingLife - casing.age) / 0.25f : 1.0f;
			casing.mesh->set_position(casing.x, casing.h, -casing.y);
			casing.mesh->set_rotation(casing.turn, casing.turn * 0.5f, 1.2f);
			casing.mesh->set_scale(kCasingScale * shrink);
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
			m_graphics->draw_particle(Pakal::tmath::vector3df(puff.x, puff.h + radius * 0.5f, -puff.y), radius, abgr, puff.additive);
		}
		m_puffs.erase(std::remove_if(m_puffs.begin(), m_puffs.end(), [](const Puff& puff) { return puff.age >= puff.life; }), m_puffs.end());
	}
}
