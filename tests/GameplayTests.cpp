///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: GameplayTests.cpp
//
// Purpose: Gameplay core on a real level layout: navigation, enemy chase and attack, and shots
//          (ray casts) at the enemy and at the walls.
//
// Usage: GameplayTests <collision.txt> <spawn.txt> <entities.txt>
//   collision.txt  "rect cx cy width height name" and "circle cx cy diameter name" (100 units = 1 m)
//   spawn.txt      "spawn x y name"
//   entities.txt   "enemy x y speed name" (other lines are ignored)
//
// Exit code 0 when every check passes.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/EnemyAI2D.h"
#include "topdown/Health.h"
#include "gameplay/NavGrid2D.h"
#include "box2D/CharacterController2D.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "math/Vec2.h"

using namespace Pakal;
using namespace Pakal::TopDown;

namespace
{
	const float kUnitsPerMeter = 100.0f;
	const float kStep = 1.0f / 60.0f;
	const uint16 kWallCategory = 0x0001;
	const uint16 kPlayerCategory = 0x0002;
	const uint16 kEnemyCategory = 0x0004;
	const float kPlayerRadius = 0.3f;
	const float kShotDamage = 40.0f;

	// A hitscan shot: the first wall or enemy along 'direction' within 'range' meters
	RayCastHit shoot(PhysicsWorld2D& world, const Pakal::Vec2& from, Pakal::Vec2 direction, float range = 30.0f)
	{
		if (direction.Normalize() <= 0.0f) return RayCastHit();
		return world.raycast(from, from + range * direction, kWallCategory | kEnemyCategory);
	}

	int g_failures = 0;

	void check(bool condition, const char* description)
	{
		std::printf("[%s] %s\n", condition ? "PASS" : "FAIL", description);
		if (!condition)
		{
			++g_failures;
		}
	}

	struct Line
	{
		std::string kind;
		float x = 0.0f, y = 0.0f, a = 0.0f, b = 0.0f;
		std::string name;
	};

	std::vector<Line> read_file(const std::string& path, const std::string& kind)
	{
		std::vector<Line> out;
		std::ifstream file(path);
		std::string text;
		while (std::getline(file, text))
		{
			std::istringstream in(text);
			Line entry;
			in >> entry.kind;
			if (entry.kind != kind)
			{
				continue;
			}
			if (kind == "rect" || kind == "enemy")
			{
				in >> entry.x >> entry.y >> entry.a;
				if (kind == "rect")
				{
					in >> entry.b;
				}
			}
			else
			{
				in >> entry.x >> entry.y;
				if (kind == "circle")
				{
					in >> entry.a;
				}
			}
			std::getline(in, entry.name);
			out.push_back(entry);
		}
		return out;
	}

	Pakal::Vec2 to_m(float x, float y)
	{
		return Pakal::Vec2(x / kUnitsPerMeter, y / kUnitsPerMeter);
	}

	// A walkable point at 'distance' from 'center' that sees 'center' without a wall in between
	bool find_spot(const NavGrid2D& nav, PhysicsWorld2D& world, const Pakal::Vec2& center, float distance, Pakal::Vec2& spot)
	{
		for (int deg = 0; deg < 360; deg += 5)
		{
			const float a = deg * 3.14159265f / 180.0f;
			const Pakal::Vec2 candidate(center.x + distance * std::cos(a), center.y + distance * std::sin(a));
			if (!nav.is_walkable(candidate))
			{
				continue;
			}
			const RayCastHit hit = world.raycast(candidate, center, kWallCategory);
			if (!hit.hit)
			{
				spot = candidate;
				return true;
			}
		}
		return false;
	}

	// The grid built by blocks (one query answers an open block) is the grid of one query a cell: random boxes, circles
	// and triangles, every cell compared with the direct query
	void check_block_build()
	{
		PhysicsWorld2D world(Pakal::Vec2(0.0f, 0.0f));
		uint32_t seed = 12345u;
		auto random = [&seed](float lo, float hi)
		{
			seed = seed * 1664525u + 1013904223u;
			return lo + (hi - lo) * float(seed >> 8) * (1.0f / 16777216.0f);
		};
		for (int i = 0; i < 60; ++i)
		{
			const Pakal::Vec2 at(random(0.0f, 120.0f), random(0.0f, 90.0f));
			switch (i % 3)
			{
			case 0: world.add_static_box(at, Pakal::Vec2(random(0.2f, 3.0f), random(0.2f, 3.0f)), kWallCategory, 0, random(0.0f, 3.0f)); break;
			case 1: world.add_static_circle(at, random(0.2f, 2.5f), kWallCategory); break;
			default:
				world.add_static_polygon({ at, at + Pakal::Vec2(random(1.0f, 4.0f), 0.0f), at + Pakal::Vec2(0.0f, random(1.0f, 4.0f)) }, kWallCategory);
			}
		}
		const Pakal::Vec2 minCorner(-3.0f, -3.0f), maxCorner(123.0f, 93.0f);
		const float cell = 0.5f, radius = 0.3f;
		NavGrid2D nav(cell);
		nav.build(world, minCorner, maxCorner, radius, kWallCategory);
		int differ = 0, blocked = 0;
		for (int y = 0; y < nav.get_height(); ++y)
		{
			for (int x = 0; x < nav.get_width(); ++x)
			{
				const Pakal::Vec2 center(minCorner.x + (x + 0.5f) * cell, minCorner.y + (y + 0.5f) * cell);
				const bool free = !world.static_within(center, radius, kWallCategory);
				blocked += free ? 0 : 1;
				differ += nav.is_walkable(center) == free ? 0 : 1;
			}
		}
		check(blocked > 100 && differ == 0, "the grid built by blocks matches one query a cell");
	}

	// A closed outline (a sea's, a river's) leaves its inside walkable but cut off: a target in it is reached at its
	// shore, a target inside it from outside is no path
	void check_closed_outline()
	{
		PhysicsWorld2D world(Pakal::Vec2(0.0f, 0.0f));
		world.add_static_chain({ Pakal::Vec2(10.0f, -10.0f), Pakal::Vec2(40.0f, -10.0f), Pakal::Vec2(40.0f, 30.0f), Pakal::Vec2(10.0f, 30.0f) }, true, kWallCategory);
		NavGrid2D nav(0.5f);
		nav.build(world, Pakal::Vec2(0.0f, -15.0f), Pakal::Vec2(45.0f, 35.0f), 0.3f);
		std::vector<Pakal::Vec2> route;
		check(nav.find_path(Pakal::Vec2(2.0f, 10.0f), Pakal::Vec2(10.6f, 10.0f), route) && !route.empty() && route.back().x < 10.0f,
			"a target just inside a closed outline is reached at its shore");
		check(!nav.find_path(Pakal::Vec2(2.0f, 10.0f), Pakal::Vec2(25.0f, 10.0f), route), "a target deep inside a closed outline has no path");
	}
}

int main(int argc, char* argv[])
{
	// Unbuffered: every early return keeps its output
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	check_closed_outline();
	check_block_build();
	if (argc < 4)
	{
		std::fprintf(stderr, "Usage: GameplayTests <collision.txt> <spawn.txt> <entities.txt>\n");
		return 2;
	}

	const auto rects = read_file(argv[1], "rect");
	const auto circles = read_file(argv[1], "circle");
	const auto spawns = read_file(argv[2], "spawn");
	const auto enemies = read_file(argv[3], "enemy");
	if (spawns.empty() || enemies.empty())
	{
		std::fprintf(stderr, "level data missing: need a spawn and at least one enemy\n");
		return 1;
	}

	// World: static walls, then navigation grid over the whole level
	PhysicsWorld2D world(Pakal::Vec2(0.0f, 0.0f));
	Pakal::Vec2 minCorner(1e9f, 1e9f), maxCorner(-1e9f, -1e9f);
	auto extend = [&](const Pakal::Vec2& p)
	{
		minCorner.x = std::min(minCorner.x, p.x);
		minCorner.y = std::min(minCorner.y, p.y);
		maxCorner.x = std::max(maxCorner.x, p.x);
		maxCorner.y = std::max(maxCorner.y, p.y);
	};
	for (const auto& r : rects)
	{
		const float hx = 0.5f * r.a / kUnitsPerMeter;
		const float hy = 0.5f * r.b / kUnitsPerMeter;
		world.add_static_box(to_m(r.x, r.y), Pakal::Vec2(hx, hy), kWallCategory);
		extend(Pakal::Vec2(to_m(r.x, r.y).x - hx, to_m(r.x, r.y).y - hy));
		extend(Pakal::Vec2(to_m(r.x, r.y).x + hx, to_m(r.x, r.y).y + hy));
	}
	for (const auto& c : circles)
	{
		const float radius = 0.5f * c.a / kUnitsPerMeter;
		world.add_static_circle(to_m(c.x, c.y), radius, kWallCategory);
		extend(Pakal::Vec2(to_m(c.x, c.y).x - radius, to_m(c.x, c.y).y - radius));
		extend(Pakal::Vec2(to_m(c.x, c.y).x + radius, to_m(c.x, c.y).y + radius));
	}
	// Walkable area also covers the player start, which may lie outside the walls' bounds
	extend(to_m(spawns[0].x, spawns[0].y));

	const Pakal::Vec2 margin(10.0f, 10.0f);
	NavGrid2D nav(0.5f);
	nav.build(world, minCorner - margin, maxCorner + margin, 0.3f);
	std::printf("nav grid: %d x %d cells\n", nav.get_width(), nav.get_height());

	// Player at the spawn
	const Pakal::Vec2 spawn = to_m(spawns[0].x, spawns[0].y);
	auto player = std::make_unique<CharacterController2D>(world, spawn, kPlayerRadius, kPlayerCategory, 0xFFFF, 0);

	// 1. Navigation: a route exists from a spot near the player to the player
	std::printf("spawn walkable: %d, distance to nearest wall %.2f m\n", nav.is_walkable(spawn) ? 1 : 0,
		world.distance_to_static(spawn));
	Pakal::Vec2 enemySpot;
	const bool spotFound = find_spot(nav, world, spawn, 8.0f, enemySpot);
	check(spotFound, "found a walkable spot 8 m from the spawn with a clear view of it");
	if (!spotFound)
	{
		std::printf("FAILED (%d failures)\n", g_failures);
		return 1;
	}
	std::vector<Pakal::Vec2> route;
	check(nav.find_path(enemySpot, spawn, route) && route.size() >= 2, "A* finds a route between two walkable points");

	// 2. Chase: the enemy sees the player, approaches, attacks, and never enters a wall
	EnemyConfig config;
	config.bodyCategory = kEnemyCategory;
	config.sightBlockers = kWallCategory;
	const uintptr_t enemyId = 1;
	auto enemy = std::make_unique<EnemyAI2D>(world, enemySpot, config, enemyId);
	float minDistance = 1e9f;
	float worstClearance = config.radius;
	float totalDamage = 0.0f;
	bool sawChase = false;
	const int steps = static_cast<int>(15.0f / kStep);
	for (int i = 0; i < steps; ++i)
	{
		totalDamage += enemy->update(kStep, spawn, nav, world);
		if (enemy->get_state() == EnemyState::Chase)
		{
			sawChase = true;
		}
		world.step(kStep);
		const float d = (enemy->get_position() - spawn).Length();
		minDistance = std::min(minDistance, d);
		worstClearance = std::min(worstClearance, world.distance_to_static(enemy->get_position()));
	}
	std::printf("chase: closest %.2f m to the player, damage dealt %.0f, closest wall surface %.2f m\n",
		minDistance, totalDamage, worstClearance);
	check(sawChase, "enemy switches to chase when it sees the player");
	check(minDistance <= config.attackRange + 0.3f, "enemy reaches attack range of the player");
	check(totalDamage > 0.0f, "enemy deals damage in range");
	check(worstClearance >= config.radius - 0.02f, "enemy never overlaps a wall while chasing");

	// 3. Shots: a shot at the enemy hits it, a shot at a wall does not
	const Pakal::Vec2 playerPos = player->get_position();
	const Pakal::Vec2 toEnemy = enemy->get_position() - playerPos;
	const RayCastHit shot = shoot(world, playerPos, toEnemy);
	check(shot.hit && shot.userData == enemyId, "shot toward the enemy hits the enemy");
	if (shot.hit && shot.userData == enemyId)
	{
		enemy->apply_damage(kShotDamage);
	}

	// Aim at the nearest wall: the shot must stop at the wall, which is not an enemy hit
	const Line* wallTarget = nullptr;
	float wallDistance = 1e9f;
	for (const auto& r : rects)
	{
		const float d = (to_m(r.x, r.y) - playerPos).Length();
		if (d > 1.0f && d < wallDistance)
		{
			wallDistance = d;
			wallTarget = &r;
		}
	}
	if (wallTarget)
	{
		// the nearest wall is ~35 m away, beyond the default 30 m range
		const RayCastHit wallShot = shoot(world, playerPos, to_m(wallTarget->x, wallTarget->y) - playerPos, 60.0f);
		std::printf("wall shot at %.2f m: hit=%d category=0x%04x userData=%llu\n", wallDistance, wallShot.hit ? 1 : 0,
			static_cast<unsigned>(wallShot.categoryBits), static_cast<unsigned long long>(wallShot.userData));
		check(wallShot.hit && (wallShot.categoryBits & kWallCategory) != 0 && wallShot.userData == 0,
			"a shot at a wall hits the wall, not an enemy");
	}

	// Keep shooting until the enemy dies (40 damage per shot, 100 health)
	int shots = 1;
	while (!enemy->is_dead() && shots < 10)
	{
		const RayCastHit again = shoot(world, playerPos, enemy->get_position() - playerPos);
		if (again.hit && again.userData == enemyId)
		{
			enemy->apply_damage(kShotDamage);
		}
		++shots;
	}
	check(enemy->is_dead(), "enemy dies after repeated hits");
	std::printf("enemy killed after %d shots, health %.0f\n", shots, enemy->health.current);

	player.reset();
	enemy.reset();
	std::printf("%s (%d failures)\n", g_failures == 0 ? "ALL PASSED" : "FAILED", g_failures);
	std::fflush(stdout);
	return g_failures == 0 ? 0 : 1;
}
