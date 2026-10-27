///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: WorldOverlays.cpp
// Purpose: What Zombies draws over the level each frame - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "WorldOverlays.h"
#include "Components.h"
#include "GameRules.h"
#include "ZombiesGame.h"
#include "topdown/view/CharacterViewComponent.h"
#include "topdown/view/RenderSpace.h"

#include "bgfx/BgfxGraphicsSystem.h"
#include "box2D/PhysicsWorld2D.h"

#include <box2d/box2d.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Zombies
{
	namespace tmath = Pakal::tmath;

	void draw_world_overlays(const OverlayScene& scene)
	{
		// Solid shapes for markers and pickups (lines alone were too faint from the camera's height). Faces turned away
		// from the sky are drawn darker, so the shape reads as a solid.
		auto shade = [](uint32_t color, float k)
		{
			const uint32_t r = uint32_t((color & 0xff) * k), g = uint32_t(((color >> 8) & 0xff) * k), b = uint32_t(((color >> 16) & 0xff) * k);
			return (color & 0xff000000) | (b << 16) | (g << 8) | r;
		};
		// A gem: an octahedron of half width r at height h, turned by 'spin'
		auto solid_gem = [&](const b2Vec2& c, float h, float r, float spin, uint32_t color)
		{
			const tmath::vector3df top = floor_point(c, h + r * 1.3f), bottom = floor_point(c, h - r * 1.3f);
			tmath::vector3df ring[4];
			for (int k = 0; k < 4; ++k)
			{
				const float a = spin + k * 1.5707963f;
				ring[k] = floor_point(c + r * b2Vec2(std::cos(a), std::sin(a)), h);
			}
			for (int k = 0; k < 4; ++k)
			{
				const int n = (k + 1) % 4;
				scene.graphics.draw_triangle3d(top, ring[k], ring[n], shade(color, k % 2 ? 1.0f : 0.82f));
				scene.graphics.draw_triangle3d(bottom, ring[n], ring[k], shade(color, k % 2 ? 0.55f : 0.45f));
			}
		};
		// A flat cross (two bars of length 2 r) at height h, turned by 'spin'
		auto solid_cross = [&](const b2Vec2& c, float h, float r, float spin, uint32_t color)
		{
			const float w = r * 0.32f;
			for (int k = 0; k < 2; ++k)
			{
				const float a = spin + k * 1.5707963f;
				const b2Vec2 u(std::cos(a), std::sin(a)), v(-u.y, u.x);
				const tmath::vector3df p0 = floor_point(c - r * u - w * v, h), p1 = floor_point(c + r * u - w * v, h);
				const tmath::vector3df p2 = floor_point(c + r * u + w * v, h), p3 = floor_point(c - r * u + w * v, h);
				scene.graphics.draw_triangle3d(p0, p1, p2, color);
				scene.graphics.draw_triangle3d(p0, p2, p3, color);
				// a darker copy just below: the cross has some thickness
				const tmath::vector3df q0 = floor_point(c - r * u - w * v, h - 0.08f), q1 = floor_point(c + r * u - w * v, h - 0.08f);
				const tmath::vector3df q2 = floor_point(c + r * u + w * v, h - 0.08f), q3 = floor_point(c - r * u + w * v, h - 0.08f);
				scene.graphics.draw_triangle3d(q0, q1, q2, shade(color, 0.5f));
				scene.graphics.draw_triangle3d(q0, q2, q3, shade(color, 0.5f));
			}
		};
		const double seconds = scene.game.timeMs / 1000.0;
		// A hurt zombie's health bar (GameUI::UpdateObjectHealthbars): over its head, across the screen, a few stacked lines
		Vec3 across{ scene.camera.target().x - scene.camera.eye().x, 0.0f, scene.camera.target().z - scene.camera.eye().z };
		{
			const float length = std::sqrt(across.x * across.x + across.z * across.z);
			across = length > 0.0001f ? Vec3{ -across.z / length, 0.0f, across.x / length } : Vec3{ 1.0f, 0.0f, 0.0f };
		}
		for (const auto& actor : scene.game.actors())
		{
			const EnemyBrainComponent* brain = actor->get<EnemyBrainComponent>();
			const CharacterViewComponent* view = actor->get<CharacterViewComponent>();
			if (brain && view && brain->health_bar_seconds() > 0.0f)
			{
				const b2Vec2 at = actor->drawn_position(scene.alpha);
				const float half = 0.45f, top = view->height() + 0.35f;
				const float fill = -half + 2.0f * half * brain->health_share();
				for (int line = 0; line < 4; ++line)
				{
					const float y = top + 0.025f * static_cast<float>(line);
					auto point = [&](float t) { return tmath::vector3df(at.x + across.x * t, y, -at.y + across.z * t); };
					if (fill > -half) scene.graphics.draw_line3d(point(-half), point(fill), 0xff30e040, false);
					if (fill < half) scene.graphics.draw_line3d(point(fill), point(half), 0xff2020c0, false);
				}
			}
			// A zombie that took money from the floor carries it over its head: kill it and the money comes back
			if (brain && view && !brain->is_dead() && brain->coins_carried() > 0)
			{
				const b2Vec2 at = actor->drawn_position(scene.alpha);
				const float bob = 0.1f * static_cast<float>(std::sin(seconds * 5.0));
				solid_gem(at, view->height() + 0.75f + bob, 0.3f, static_cast<float>(seconds * 4.0), 0xff20d0ff);
			}
		}
		// The survivor of an escort mission: its health over its head, always, and a ring under it so it is found in a crowd
		if (const Actor* ally = scene.game.escorted ? scene.game.find(scene.game.escorted) : nullptr)
		{
			const HealthComponent* health = ally->get<HealthComponent>();
			const CharacterViewComponent* view = ally->get<CharacterViewComponent>();
			if (health && !health->is_dead())
			{
				const b2Vec2 at = ally->drawn_position(scene.alpha);
				const float half = 0.5f, top = (view ? view->height() : 1.7f) + 0.35f;
				const float fill = -half + 2.0f * half * std::max(0.0f, health->current() / std::max(1.0f, health->maximum()));
				for (int line = 0; line < 5; ++line)
				{
					const float y = top + 0.025f * static_cast<float>(line);
					auto point = [&](float t) { return tmath::vector3df(at.x + across.x * t, y, -at.y + across.z * t); };
					if (fill > -half) scene.graphics.draw_line3d(point(-half), point(fill), 0xffffc040, false);
					if (fill < half) scene.graphics.draw_line3d(point(fill), point(half), 0xff2020c0, false);
				}
				for (int i = 0; i < 24; ++i)
				{
					const float a0 = i * 6.2831853f / 24, a1 = (i + 1) * 6.2831853f / 24;
					const b2Vec2 d0(std::cos(a0), std::sin(a0)), d1(std::cos(a1), std::sin(a1));
					scene.graphics.draw_triangle3d(floor_point(at + 0.75f * d0, 0.05f), floor_point(at + 0.75f * d1, 0.05f), floor_point(at + 0.6f * d1, 0.05f), 0xffffc040);
					scene.graphics.draw_triangle3d(floor_point(at + 0.75f * d0, 0.05f), floor_point(at + 0.6f * d1, 0.05f), floor_point(at + 0.6f * d0, 0.05f), 0xffffc040);
				}
			}
		}
		// A defend mission: a bar over each barricade that is not whole (yellow while broken: stand next to it to put it back)
		// and over the thing to defend
		for (const auto& actor : scene.game.actors())
		{
			const DestructibleComponent* prop = actor->get<DestructibleComponent>();
			if (!prop || !(prop->rules().repairable || prop->rules().objective)) continue;
			const float share = prop->repair_share();
			if (share >= 0.999f && !prop->rules().objective) continue;
			const b2Vec2 at = actor->position;
			const bool goal = prop->rules().objective;
			const float half = goal ? 1.2f : 0.6f, top = goal ? 3.6f : 1.6f;
			const float fill = -half + 2.0f * half * share;
			const uint32_t full = prop->is_broken() ? 0xff20d0ff : 0xff30e040;
			for (int line = 0; line < (goal ? 6 : 4); ++line)
			{
				const float y = top + 0.025f * static_cast<float>(line);
				auto point = [&](float t) { return tmath::vector3df(at.x + across.x * t, y, -at.y + across.z * t); };
				if (fill > -half) scene.graphics.draw_line3d(point(-half), point(fill), full, false);
				if (fill < half) scene.graphics.draw_line3d(point(fill), point(half), 0xff2020c0, false);
			}
		}
		for (const auto& actor : scene.game.actors())
		{
			// What lies on the floor, solid so it reads from the camera's height: money a gold gem, a token a bigger cyan one
			// that bobs, health a red cross, a boost a gem in the color the hero glows with it (speed cyan, damage orange,
			// defence blue, air strike red). They blink before they disappear.
			if (const PickupComponent* pickup = actor->get<PickupComponent>())
			{
				if (pickup->seconds_left() < 3.0f && std::fmod(seconds, 0.3) < 0.12) continue;
				const float spin = static_cast<float>(seconds * 3.0);
				const b2Vec2 c = actor->position;
				const PickupRules& rules = pickup->rules();
				if (rules.type == "Token")
				{
					const float bob = 0.15f * static_cast<float>(std::sin(seconds * 3.0));
					solid_gem(c, 0.75f + bob, 0.5f, spin, 0xffffe040);
					scene.graphics.draw_particle(tmath::vector3df(c.x, 0.75f + bob, -c.y), 1.0f, 0x60ffd040, true);
					continue;
				}
				// what the consumable does picks the shape and color: health a red cross, a boost a gem in the color the hero glows
				// with it (speed cyan, damage orange, defence blue), a bombardment red
				auto consumable = rules.consumable != 0 ? scene.rules.consumables.find(rules.consumable) : scene.rules.consumables.end();
				const ConsumableRules* gives = consumable != scene.rules.consumables.end() ? &consumable->second : nullptr;
				if (gives && gives->addHealth > 0.0f)
				{
					const float bob = 0.08f * static_cast<float>(std::sin(seconds * 4.0));
					solid_cross(c, 0.55f + bob, 0.5f, spin * 0.4f, 0xff2a2aff);
					scene.graphics.draw_particle(tmath::vector3df(c.x, 0.55f + bob, -c.y), 1.0f, 0x602a2aff, true);
					continue;
				}
				if (rules.consumable != 0)
				{
					const uint32_t color = !gives ? 0xff2020ff : gives->speedFactor > 1.0f ? 0xffffd040 : gives->damageFactor > 1.0f ? 0xff2080ff
						: gives->defenceFactor > 1.0f ? 0xffff6020 : 0xff2020ff;
					const float bob = 0.08f * static_cast<float>(std::sin(seconds * 4.0));
					solid_gem(c, 0.6f + bob, 0.42f, spin, color);
					scene.graphics.draw_particle(tmath::vector3df(c.x, 0.6f + bob, -c.y), 1.0f, (color & 0x00ffffff) | 0x60000000, true);
					continue;
				}
				solid_gem(c, 0.45f, 0.32f, spin, 0xff20d0ff);
				scene.graphics.draw_particle(tmath::vector3df(c.x, 0.45f, -c.y), 0.6f, 0x4020d0ff, true);
			}
			// The armed gift box: a ring of its pick-up radius
			if (const GiftBoxComponent* gift = actor->get<GiftBoxComponent>())
			{
				if (!gift->armed()) continue;
				const float radius = meters(scene.rules.giftRadius) * (0.9f + 0.1f * static_cast<float>(std::sin(seconds * 4.0)));
				for (int i = 0; i < 32; ++i)
				{
					const float a0 = i * 6.2831853f / 32, a1 = (i + 1) * 6.2831853f / 32;
					scene.graphics.draw_line3d(floor_point(actor->position + radius * b2Vec2(std::cos(a0), std::sin(a0)), 0.05f),
						floor_point(actor->position + radius * b2Vec2(std::cos(a1), std::sin(a1)), 0.05f), 0xff40ffff);
				}
			}
		}

		// The beacons of ShowMarker: a ring on the floor and a column of light that can be seen from afar
		for (int id : scene.markers)
		{
			b2Vec2 c;
			if (!scene.pointPosition(id, c)) continue;
			const float radius = 2.5f * (0.92f + 0.08f * static_cast<float>(std::sin(seconds * 5.0)));
			for (int i = 0; i < 40; ++i)
			{
				const float a0 = i * 6.2831853f / 40, a1 = (i + 1) * 6.2831853f / 40;
				const b2Vec2 d0(std::cos(a0), std::sin(a0)), d1(std::cos(a1), std::sin(a1));
				const tmath::vector3df o0 = floor_point(c + radius * d0, 0.06f), o1 = floor_point(c + radius * d1, 0.06f);
				const tmath::vector3df i0 = floor_point(c + (radius - 0.45f) * d0, 0.06f), i1 = floor_point(c + (radius - 0.45f) * d1, 0.06f);
				scene.graphics.draw_triangle3d(o0, o1, i1, 0xff40ffa0);
				scene.graphics.draw_triangle3d(o0, i1, i0, 0xff40ffa0);
			}
			solid_gem(c, 2.4f + 0.25f * static_cast<float>(std::sin(seconds * 3.0)), 0.5f, static_cast<float>(seconds * 2.0), 0xff40ffa0);
			for (int k = 0; k < 8; ++k)
			{
				const float h = std::fmod(static_cast<float>(seconds) * 2.0f + k * 0.9f, 7.2f);
				scene.graphics.draw_particle(tmath::vector3df(c.x, h, -c.y), 0.7f, 0x9040ffa0, true);
			}
		}
		// An arrow on the floor next to the player, toward the nearest beacon, or else the nearest token (they may be off the screen)
		if (const Actor* player = scene.game.find(scene.playerId))
		{
			b2Vec2 best;
			float bestDistance = 1e9f;
			auto consider = [&](const b2Vec2& c)
			{
				if ((c - player->position).Length() < bestDistance)
				{
					best = c;
					bestDistance = (c - player->position).Length();
				}
			};
			for (int id : scene.markers)
			{
				b2Vec2 c;
				if (scene.pointPosition(id, c)) consider(c);
			}
			if (scene.markers.empty())
			{
				for (const auto& actor : scene.game.actors())
				{
					const PickupComponent* pickup = actor->get<PickupComponent>();
					if (pickup && !actor->removed && pickup->rules().type == "Token") consider(actor->position);
				}
			}
			if (bestDistance > 4.0f && bestDistance < 1e8f)
			{
				b2Vec2 way = best - player->position;
				way *= 1.0f / bestDistance;
				const b2Vec2 side(-way.y, way.x);
				// a solid arrow (head and shaft) on the floor, with a dark rim so it shows on light paving too
				const float pulse = 1.0f + 0.08f * static_cast<float>(std::sin(seconds * 6.0));
				const b2Vec2 tip = player->position + 2.7f * pulse * way, base = player->position + 2.1f * pulse * way;
				const b2Vec2 tail = player->position + 1.6f * way;
				const uint32_t green = 0xff40ffa0, rim = 0xc0103020;
				auto arrow = [&](float grow, uint32_t color, float h)
				{
					const b2Vec2 t2 = tip + grow * way;
					scene.graphics.draw_triangle3d(floor_point(t2, h), floor_point(base + (0.38f + grow) * side, h), floor_point(base - (0.38f + grow) * side, h), color, false);
					const b2Vec2 s0 = base + (0.13f + grow) * side, s1 = base - (0.13f + grow) * side;
					const b2Vec2 s2 = tail - grow * way - (0.13f + grow) * side, s3 = tail - grow * way + (0.13f + grow) * side;
					scene.graphics.draw_triangle3d(floor_point(s0, h), floor_point(s1, h), floor_point(s2, h), color, false);
					scene.graphics.draw_triangle3d(floor_point(s0, h), floor_point(s2, h), floor_point(s3, h), color, false);
				};
				arrow(0.07f, rim, 0.07f);
				arrow(0.0f, green, 0.09f);
			}
		}

		// Where the mouse aims
		const Actor* player = scene.game.find(scene.playerId);
		b2Vec2 aim;
		if (player && scene.controls.mouse_aim(scene.camera, static_cast<float>(scene.graphics.get_width()), static_cast<float>(scene.graphics.get_height()), aim))
		{
			const float r = 0.25f, h = scene.controls.aim_height();
			scene.graphics.draw_line3d(floor_point(aim + b2Vec2(-r, 0), h), floor_point(aim + b2Vec2(r, 0), h), 0xff4040ff, false);
			scene.graphics.draw_line3d(floor_point(aim + b2Vec2(0, -r), h), floor_point(aim + b2Vec2(0, r), h), 0xff4040ff, false);
		}

		if (!scene.showColliders && std::getenv("ZOMBIES_SHOW_COLLIDERS") == nullptr) return;
		for (const b2Body* body = scene.world.native()->GetBodyList(); body; body = body->GetNext())
		{
			if (!body->IsEnabled()) continue;
			for (const b2Fixture* fixture = body->GetFixtureList(); fixture; fixture = fixture->GetNext())
			{
				const uint32_t color = body->GetType() == b2_staticBody ? 0xffffc040 : 0xff40ff40;
				auto line = [&](const b2Vec2& a, const b2Vec2& b)
				{
					scene.graphics.draw_line3d(floor_point(b2Mul(body->GetTransform(), a), 0.08f), floor_point(b2Mul(body->GetTransform(), b), 0.08f), color, false);
				};
				const b2Shape* shape = fixture->GetShape();
				if (shape->GetType() == b2Shape::e_edge)
				{
					const auto* edge = static_cast<const b2EdgeShape*>(shape);
					line(edge->m_vertex1, edge->m_vertex2);
				}
				else if (shape->GetType() == b2Shape::e_polygon)
				{
					const auto* polygon = static_cast<const b2PolygonShape*>(shape);
					for (int i = 0; i < polygon->m_count; ++i)
						line(polygon->m_vertices[i], polygon->m_vertices[(i + 1) % polygon->m_count]);
				}
				else if (shape->GetType() == b2Shape::e_circle)
				{
					const auto* circle = static_cast<const b2CircleShape*>(shape);
					for (int i = 0; i < 24; ++i)
					{
						const float a = i * 6.2831853f / 24, b = (i + 1) * 6.2831853f / 24;
						line(circle->m_p + circle->m_radius * b2Vec2(std::cos(a), std::sin(a)),
							circle->m_p + circle->m_radius * b2Vec2(std::cos(b), std::sin(b)));
					}
				}
			}
		}
	}
}
