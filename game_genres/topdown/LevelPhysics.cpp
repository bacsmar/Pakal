///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelPhysics.cpp
// Purpose: A level's physics from its document - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/LevelPhysics.h"

#include "LevelDocument.h"
#include "box2D/PhysicsWorld2D.h"
#include "gameplay/NavGrid2D.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace Pakal::TopDown
{
	LevelPhysicsCount build_level_physics(const Pakal::LevelDocument& doc, Pakal::PhysicsWorld2D& world, Pakal::NavGrid2D& nav,
		const TopDownLayers& layers, float agentRadius, const std::function<uintptr_t(int gameId)>& wallOwner,
		const std::function<bool(uintptr_t actor)>& isBarricade)
	{
		LevelPhysicsCount count;
		b2Vec2 minCorner(1e9f, 1e9f), maxCorner(-1e9f, -1e9f);
		auto extend = [&](float x, float y)
		{
			minCorner.x = std::min(minCorner.x, x);
			minCorner.y = std::min(minCorner.y, y);
			maxCorner.x = std::max(maxCorner.x, x);
			maxCorner.y = std::max(maxCorner.y, y);
		};

		// Props' own collision: ray casts report the prop actor, so bullets can break it
		for (const auto& wall : doc.walls)
		{
			const uintptr_t userData = wallOwner ? wallOwner(wall.id) : 0;
			const bool barricade = userData && isBarricade && isBarricade(userData);
			const uint16_t category = barricade ? layers.barricade : layers.wall;
			const float x = doc.meters(wall.x);
			const float y = doc.meters(wall.y);
			if (wall.shape == "rect")
			{
				const float hx = doc.meters(wall.w) * 0.5f;
				const float hy = doc.meters(wall.h) * 0.5f;
				// a turned box (a car along a diagonal street): its own extent, not the one of the box around it
				const float angle = static_cast<float>(wall.angle) * 3.14159265f / 180.0f;
				world.add_static_box(b2Vec2(x, y), b2Vec2(hx, hy), category, userData, angle);
				const float c = std::abs(std::cos(angle)), s = std::abs(std::sin(angle));
				const float ex = hx * c + hy * s, ey = hx * s + hy * c;
				extend(x - ex, y - ey);
				extend(x + ex, y + ey);
				++count.walls;
			}
			else if (wall.shape == "circle")
			{
				const float r = doc.meters(wall.d) * 0.5f;
				world.add_static_circle(b2Vec2(x, y), r, category, userData);
				extend(x - r, y - r);
				extend(x + r, y + r);
				++count.walls;
			}
		}

		// The level's collision polygons. Solid ones block everyone; "player" ones block only the player.
		// Sand and water do not block (water only changes the footsteps in the game).
		if (doc.collision.contains("segments") && doc.collision["segments"].is_array())
		{
			for (const auto& segment : doc.collision["segments"])
			{
				const std::string layer = segment.value("layer", std::string());
				if (layer != "solid" && layer != "player")
				{
					continue;
				}
				std::vector<b2Vec2> points;
				for (const auto& point : segment.value("points", Pakal::Json::array()))
				{
					points.push_back(b2Vec2(doc.meters(point[0].get<double>()), doc.meters(point[1].get<double>())));
					extend(points.back().x, points.back().y);
				}
				if (segment.value("filled", false))
				{
					world.add_static_polygon(points, layer == "player" ? layers.playerWall : layers.wall, layer == "player" ? layers.player : 0xFFFF);
				}
				else
				{
					world.add_static_chain(points, true, layers.wall);
				}
				++count.outlines;
			}
		}
		const b2Vec2 spawn(doc.meters(doc.spawn[0]), doc.meters(doc.spawn[1]));
		extend(spawn.x, spawn.y);
		const b2Vec2 margin(10.0f, 10.0f);
		nav.build(world, minCorner - margin, maxCorner + margin, agentRadius, layers.wall);
		return count;
	}
}
