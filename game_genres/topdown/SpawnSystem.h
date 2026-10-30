///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SpawnSystem.h
// Purpose: Enemy spawning after the game's SetEnemySpawn rules (LegacyRuntimeDirector.cs): an order
//          keeps N enemies of a type alive; each attempt waits its cooldown (at least 0.5 s), and
//          uses a spawner with a walkable route to the player, preferring the quadrant rolled for it
//          and then the nearest. A spawner rests 5 s after it is used.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <box2d/b2_math.h>

#include <functional>
#include <set>
#include <string>
#include <vector>

namespace Pakal::TopDown
{
	class TopDownGame;

	class SpawnSystem
	{
	public:
		// Makes an enemy of 'type' at 'at'; returns its actor id, or 0 when it cannot (no model free)
		using Factory = std::function<uintptr_t(const std::string& type, const b2Vec2& at)>;

		SpawnSystem(Factory factory, float clearance);

		// 'group': the spawners can be in groups that take turns (a defend mission's sides); -1 none
		void add_point(const b2Vec2& at, int group = -1) { m_points.push_back({ at, 0.0f, group }); }
		// Only the points of this group are used (and those of none); -1: all of them
		void set_active_group(int group) { m_activeGroup = group; }
		// A group shut for good (its hole plugged), or open again
		void set_group_enabled(int group, bool on) { if (on) m_shutGroups.erase(group); else m_shutGroups.insert(group); }
		size_t point_count() const { return m_points.size(); }

		// SetEnemySpawn(type, alive, cooldownMs)
		void set_order(const std::string& type, int requested, float cooldownMs);
		void update(TopDownGame& game, float dt);

		// "25001 3/4 25009 2/4": alive / requested per order, for the log
		std::string describe() const;

	private:
		struct Point
		{
			b2Vec2 position;
			float readyAt = 0.0f; // seconds of the game clock
			int group = -1;
		};

		struct Order
		{
			std::string type;
			int requested = 0;
			float cooldownMs = 1000.0f;
			float timer = 0.0f;
			std::vector<uintptr_t> spawned; // ids of its enemies still alive
		};

		bool try_spawn(TopDownGame& game, Order& order);

		Factory m_factory;
		float m_clearance;
		std::vector<Point> m_points;
		std::vector<Order> m_orders;
		float m_clock = 0.0f;
		size_t m_nextPlayer = 0;   // the spawners aim for the players in turn
		int m_spawnedCount = 0;
		int m_activeGroup = -1;
		std::set<int> m_shutGroups;
	};
}
