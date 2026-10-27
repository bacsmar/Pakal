///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SpawnSystem.cpp
// Purpose: Enemy spawning after the game's SetEnemySpawn rules - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/SpawnSystem.h"
#include "topdown/Actor.h"
#include "topdown/TopDownGame.h"

#include "box2D/PhysicsWorld2D.h"
#include "gameplay/NavGrid2D.h"

#include <algorithm>
#include <numeric>

namespace Pakal::TopDown
{
	namespace
	{
		const float kSpawnerIntervalSeconds = 5.0f; // ZombieSpawner.SpawnIntervalTime in the game data
		const int kMoanEvery = 4;                   // EnemyComponent: every 4th zombie that appears moans
		const size_t kRouteChecks = 6;              // reachable points gathered per attempt (nearest first)
	}

	SpawnSystem::SpawnSystem(Factory factory, float clearance) : m_factory(std::move(factory)), m_clearance(clearance)
	{
	}

	void SpawnSystem::set_order(const std::string& type, int requested, float cooldownMs)
	{
		for (auto& order : m_orders)
		{
			if (order.type == type)
			{
				order.requested = requested;
				order.cooldownMs = cooldownMs;
				return;
			}
		}
		Order order;
		order.type = type;
		order.requested = requested;
		order.cooldownMs = cooldownMs;
		m_orders.push_back(order);
	}

	void SpawnSystem::update(TopDownGame& game, float dt)
	{
		m_clock += dt;
		for (auto& order : m_orders)
		{
			// An enemy that died (or is gone) no longer counts for its order
			order.spawned.erase(std::remove_if(order.spawned.begin(), order.spawned.end(), [&game](uintptr_t id)
			{
				const Actor* actor = game.find(id);
				return !actor || !game.is_alive(*actor);
			}), order.spawned.end());

			if (order.requested <= 0 || static_cast<int>(order.spawned.size()) >= order.requested)
			{
				continue;
			}
			order.timer -= dt;
			if (order.timer > 0.0f)
			{
				continue;
			}
			// As EnemyManagementSystem: the interval counts from the last zombie that came out; when no spawner is ready
			// it tries again on the next step
			if (try_spawn(game, order))
			{
				order.timer = order.cooldownMs / 1000.0f;
			}
		}
	}

	bool SpawnSystem::try_spawn(TopDownGame& game, Order& order)
	{
		// the next player in turn (there is one in most games)
		const Actor* player = nullptr;
		const auto& players = game.players();
		for (size_t i = 0; i < players.size() && !player; ++i)
		{
			const size_t index = (m_nextPlayer + i) % players.size();
			if (players[index])
			{
				player = players[index];
				m_nextPlayer = index + 1;
			}
		}
		if (m_points.empty() || !player)
		{
			return false;
		}
		const b2Vec2 target = player->position;

		// Nearest ready points first; the first few with a walkable route to the player are candidates
		std::vector<size_t> byDistance(m_points.size());
		std::iota(byDistance.begin(), byDistance.end(), size_t(0));
		std::sort(byDistance.begin(), byDistance.end(), [&](size_t a, size_t b)
		{
			return (m_points[a].position - target).LengthSquared() < (m_points[b].position - target).LengthSquared();
		});
		std::vector<size_t> reachable;
		for (size_t p : byDistance)
		{
			if (m_points[p].readyAt > m_clock || game.world().static_within(m_points[p].position, m_clearance, game.layers.wall))
			{
				continue;
			}
			std::vector<b2Vec2> route;
			if (game.nav().find_path(m_points[p].position, target, route))
			{
				reachable.push_back(p);
				if (reachable.size() >= kRouteChecks)
				{
					break;
				}
			}
		}
		if (reachable.empty())
		{
			return false;
		}

		// Each point rolls a quadrant around the player; the points in their rolled quadrant are preferred
		std::vector<size_t> preferred;
		for (size_t p : reachable)
		{
			const int quadrant = game.random("spawn").between(0, 3);
			const b2Vec2 delta = m_points[p].position - target;
			const bool inQuadrant = (quadrant == 0 && delta.x < 0 && delta.y < 0) || (quadrant == 1 && delta.x > 0 && delta.y < 0)
				|| (quadrant == 2 && delta.x < 0 && delta.y > 0) || (quadrant == 3 && delta.x > 0 && delta.y > 0);
			if (inQuadrant)
			{
				preferred.push_back(p);
			}
		}
		Point& point = m_points[(preferred.empty() ? reachable : preferred).front()]; // nearest of the candidates
		const uintptr_t id = m_factory(order.type, point.position);
		if (id == 0)
		{
			return false;
		}
		point.readyAt = m_clock + kSpawnerIntervalSeconds;
		order.spawned.push_back(id);
		if (++m_spawnedCount % kMoanEvery == 0)
		{
			game.play(game.rules().sound("enemyMoan"), point.position);
		}
		return true;
	}

	std::string SpawnSystem::describe() const
	{
		std::string text;
		for (const auto& order : m_orders)
		{
			text += (text.empty() ? "" : " ") + order.type + " " + std::to_string(order.spawned.size()) + "/" + std::to_string(order.requested);
		}
		return text;
	}
}
