///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PhysicsTests.cpp
//
// Purpose: Top-down physics checks on the engine's 2D world: static collision, a character that
//          walks along walls without entering them, and ray casts.
//
// Usage: PhysicsTests <collision.txt> <spawn.txt>
//   collision.txt  Lines "rect cx cy width height name" and "circle cx cy diameter name", in level units
//   spawn.txt      Lines "spawn x y name" (the first one is used as the start point)
//   One level unit is 1 centimeter (100 units per meter).
//
// Exit code 0 when every check passes.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "box2D/PhysicsWorld2D.h"
#include "box2D/CharacterController2D.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "math/Vec2.h"

using namespace Pakal;

namespace
{
	const float kUnitsPerMeter = 100.0f;
	const uint16 kWallCategory = 0x0001;
	const uint16 kCharacterCategory = 0x0002;
	const float kCharacterRadius = 0.30f; // 30 units
	const float kOverlapTolerance = 0.02f; // 2 cm

	int g_failures = 0;

	void check(bool condition, const char* description)
	{
		std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", description);
		if (!condition)
		{
			++g_failures;
		}
	}

	struct LevelLine
	{
		std::string kind;
		float x = 0.0f;
		float y = 0.0f;
		float a = 0.0f; // width or diameter
		float b = 0.0f; // height
		std::string name;
	};

	std::vector<LevelLine> read_lines(const std::string& path, const std::string& kind)
	{
		std::vector<LevelLine> out;
		std::ifstream file(path);
		std::string line;
		while (std::getline(file, line))
		{
			std::istringstream in(line);
			LevelLine entry;
			in >> entry.kind;
			if (entry.kind != kind)
			{
				continue;
			}
			if (kind == "rect")
			{
				in >> entry.x >> entry.y >> entry.a >> entry.b;
			}
			else
			{
				in >> entry.x >> entry.y >> entry.a;
			}
			std::getline(in, entry.name);
			out.push_back(entry);
		}
		return out;
	}

	Pakal::Vec2 to_meters(float x, float y)
	{
		return Pakal::Vec2(x / kUnitsPerMeter, y / kUnitsPerMeter);
	}

	// Walks in one direction for some seconds. Reports the distance traveled and the smallest gap
	// between the character center and any wall surface during the walk.
	float walk(PhysicsWorld2D& world, CharacterController2D& character, const Pakal::Vec2& direction,
		float seconds, float& travelled)
	{
		const float step = 1.0f / 60.0f;
		const int steps = static_cast<int>(seconds / step);
		const Pakal::Vec2 start = character.get_position();
		float minClearance = world.distance_to_static(character.get_position());

		character.set_move(direction, 5.0f); // 5 m/s
		for (int i = 0; i < steps; ++i)
		{
			character.update();
			world.step(step);
			const float clearance = world.distance_to_static(character.get_position());
			if (clearance < minClearance)
			{
				minClearance = clearance;
			}
		}

		travelled = (character.get_position() - start).Length();
		character.set_move(Pakal::Vec2(0.0f, 0.0f), 0.0f);
		character.update();
		return minClearance;
	}
}

int main(int argc, char* argv[])
{
	if (argc < 3)
	{
		std::cerr << "Usage: PhysicsTests <collision.txt> <spawn.txt>" << std::endl;
		return 2;
	}

	const auto rects = read_lines(argv[1], "rect");
	const auto circles = read_lines(argv[1], "circle");
	const auto spawns = read_lines(argv[2], "spawn");

	std::printf("loaded: %zu rects, %zu circles, %zu spawns\n", rects.size(), circles.size(), spawns.size());
	check(!rects.empty(), "collision file has rectangles");
	check(!spawns.empty(), "spawn file has a start point");
	if (spawns.empty())
	{
		std::fflush(stdout);
		return 1;
	}

	PhysicsWorld2D world(Pakal::Vec2(0.0f, 0.0f)); // top-down
	for (const auto& r : rects)
	{
		world.add_static_box(to_meters(r.x, r.y), Pakal::Vec2(0.5f * r.a / kUnitsPerMeter, 0.5f * r.b / kUnitsPerMeter), kWallCategory);
	}
	for (const auto& c : circles)
	{
		world.add_static_circle(to_meters(c.x, c.y), 0.5f * c.a / kUnitsPerMeter, kWallCategory);
	}

	const Pakal::Vec2 start = to_meters(spawns[0].x, spawns[0].y);
	auto character = std::make_unique<CharacterController2D>(world, start, kCharacterRadius,
		kCharacterCategory, 0xFFFF);

	// 1. Walking in four directions, two seconds each: the center never gets closer than the radius
	const Pakal::Vec2 directions[] = { Pakal::Vec2(1, 0), Pakal::Vec2(0, 1), Pakal::Vec2(-1, 0), Pakal::Vec2(0, -1) };
	const char* names[] = { "east", "north", "west", "south" };
	float worstClearance = kCharacterRadius;
	float totalTravelled = 0.0f;
	for (int i = 0; i < 4; ++i)
	{
		float travelled = 0.0f;
		const float clearance = walk(world, *character, directions[i], 2.0f, travelled);
		totalTravelled += travelled;
		if (clearance < worstClearance)
		{
			worstClearance = clearance;
		}
		std::printf("walk %-5s travelled %.2f m, end (%.2f, %.2f), closest surface %.2f m\n", names[i],
			travelled, character->get_position().x, character->get_position().y, clearance);
	}
	check(worstClearance >= kCharacterRadius - kOverlapTolerance, "character never overlaps a wall while walking");
	check(totalTravelled > 0.5f, "character moves when given a direction");

	// 2. Ray toward the nearest wall, from the character
	const Pakal::Vec2 from = character->get_position();
	const LevelLine* target = nullptr;
	float bestDistance = 1e9f;
	for (const auto& r : rects)
	{
		const float d = (to_meters(r.x, r.y) - from).Length();
		if (d > 1.0f && d < bestDistance)
		{
			bestDistance = d;
			target = &r;
		}
	}
	if (!target)
	{
		check(false, "found a wall to aim at");
		std::printf("FAILED (%d failures)\n", g_failures);
		std::fflush(stdout);
		return 1;
	}

	const RayCastHit toward = world.raycast(from, to_meters(target->x, target->y), kWallCategory);
	std::printf("ray to '%s' at %.2f m: hit=%d static=%d fraction=%.3f\n", target->name.c_str(), bestDistance,
		toward.hit ? 1 : 0, toward.staticBody ? 1 : 0, toward.fraction);
	check(toward.hit && toward.staticBody, "ray toward a wall hits a static body");
	check(toward.hit && toward.fraction < 1.0f, "ray toward a wall stops before its far end");

	// 3. Walk into that wall: the character stops at its surface and never passes through it
	if (toward.hit)
	{
		Pakal::Vec2 dir = toward.point - from;
		const float distanceToWall = dir.Normalize();
		float travelled = 0.0f;
		const float clearance = walk(world, *character, dir, 8.0f, travelled); // 40 m if unobstructed
		const float gap = world.distance_to_static(character->get_position());
		std::printf("walk into wall: %.2f m away, travelled %.2f m, closest surface %.2f m\n",
			distanceToWall, travelled, gap);

		check(clearance >= kCharacterRadius - kOverlapTolerance, "character never overlaps a wall when walking into it");
		check(travelled > 0.5f, "character moves toward the wall");
		// Walking diagonally against a wall slides along it (around corners too), so the walk does not
		// necessarily end at the wall surface. What must hold is that it never ends inside a wall.
		check(gap >= 0.0f, "character does not end inside a wall");
	}

	// 4. Ray toward the nearest enemy (a circle in the level data): the hit lies on its surface
	const LevelLine* enemy = nullptr;
	float enemyDistance = 1e9f;
	for (const auto& c : circles)
	{
		const float d = (to_meters(c.x, c.y) - from).Length();
		if (d < enemyDistance)
		{
			enemyDistance = d;
			enemy = &c;
		}
	}
	if (enemy)
	{
		const Pakal::Vec2 center = to_meters(enemy->x, enemy->y);
		const float radius = 0.5f * enemy->a / kUnitsPerMeter;
		const RayCastHit seen = world.raycast(from, center, kWallCategory);
		const float surfaceError = seen.hit ? std::fabs((seen.point - center).Length() - radius) : 1e9f;
		std::printf("ray to enemy '%s' at %.2f m: hit=%d surface error %.3f m\n", enemy->name.c_str(),
			enemyDistance, seen.hit ? 1 : 0, surfaceError);
		check(seen.hit && surfaceError < 0.05f, "ray toward an enemy stops on its surface");
	}

	// 5. A ray that excludes wall categories reports nothing, even with walls in the way
	const RayCastHit none = world.raycast(from, from + Pakal::Vec2(500.0f, 0.0f), kCharacterCategory);
	check(!none.hit, "ray with a mask that excludes walls reports nothing");

	character.reset();

	// 6. The world's shapes as a debug view sees them (for_each_shape), in world coordinates, and a character made on
	// the world itself (no Box2D world in the caller's hands)
	{
		PhysicsWorld2D small(Pakal::Vec2(0.0f, 0.0f));
		small.add_static_box(Pakal::Vec2(10.0f, 5.0f), Pakal::Vec2(1.0f, 2.0f), 0x0002, 77);
		small.add_static_chain({ Pakal::Vec2(0.0f, 0.0f), Pakal::Vec2(4.0f, 0.0f), Pakal::Vec2(4.0f, 3.0f) }, false, 0x0004);
		CharacterController2D walker(small, Pakal::Vec2(-3.0f, 1.0f), 0.4f, 0x0008, 0xFFFF, 5);
		// (a static chain is made of one edge per segment: solid from both sides)
		int boxes = 0, edges = 0, circles = 0;
		bool boxRight = false, edgesRight = true, circleRight = false;
		small.for_each_shape([&](const PhysicsShape2D& shape)
		{
			if (shape.kind == PhysicsShape2D::Kind::Polygon)
			{
				++boxes;
				boxRight = shape.isStatic && shape.count == 4 && shape.categoryBits == 0x0002 && shape.userData == 77 &&
					std::fabs(shape.lower.x - 9.0f) < 0.02f && std::fabs(shape.upper.y - 7.0f) < 0.02f;
			}
			else if (shape.kind == PhysicsShape2D::Kind::Edge)
			{
				++edges;
				const bool second = std::fabs(shape.points[0].x - 4.0f) < 1e-5f;
				edgesRight = edgesRight && shape.count == 2 && shape.categoryBits == 0x0004 &&
					std::fabs(shape.points[1].y - (second ? 3.0f : 0.0f)) < 1e-5f;
			}
			else if (shape.kind == PhysicsShape2D::Kind::Circle)
			{
				++circles;
				circleRight = !shape.isStatic && shape.userData == 5 && std::fabs(shape.radius - 0.4f) < 1e-5f &&
					std::fabs(shape.center.x + 3.0f) < 1e-5f && std::fabs(shape.center.y - 1.0f) < 1e-5f;
			}
		});
		check(boxes == 1 && boxRight, "for_each_shape gives a static box with its corners, category, owner and bounds");
		check(edges == 2 && edgesRight, "for_each_shape gives a static chain's segments, each with its two ends");
		check(circles == 1 && circleRight, "a character made on a PhysicsWorld2D is a circle of the world, where it was put");
	}

	// Static polygons of any shape: a 16-sided circle (more corners than Box2D's 8) and an L (concave), clockwise
	{
		Pakal::PhysicsWorld2D world(Pakal::Vec2(0.0f, 0.0f));
		std::vector<Pakal::Vec2> circle;
		for (int i = 0; i < 16; ++i)
			circle.push_back(Pakal::Vec2(10.0f + 2.0f * std::cos(i * 0.3926991f), 2.0f * std::sin(i * 0.3926991f)));
		world.add_static_polygon(circle);
		world.add_static_polygon({ {0, 0}, {0, 4}, {1, 4}, {1, 1}, {4, 1}, {4, 0} });   // an L, clockwise
		check(world.point_in_static(Pakal::Vec2(10.0f, 0.0f)) && world.point_in_static(Pakal::Vec2(11.8f, 0.1f)) &&
			world.point_in_static(Pakal::Vec2(8.2f, -0.1f)), "a static polygon of 16 corners is solid all over");
		check(!world.point_in_static(Pakal::Vec2(12.3f, 0.0f)), "a static polygon of 16 corners ends at its edge");
		check(world.point_in_static(Pakal::Vec2(0.5f, 3.5f)) && world.point_in_static(Pakal::Vec2(3.5f, 0.5f)),
			"a concave static polygon is solid in both arms");
		check(!world.point_in_static(Pakal::Vec2(2.5f, 2.5f)), "a concave static polygon is not filled to its hull");
	}

	std::printf("%s (%d failures)\n", g_failures == 0 ? "ALL PASSED" : "FAILED", g_failures);
	std::fflush(stdout);
	return g_failures == 0 ? 0 : 1;
}
