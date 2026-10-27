///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: NavGrid2D.cpp
//
// Purpose: Walkable grid and A* paths - implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "NavGrid2D.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>

namespace Pakal
{
	NavGrid2D::NavGrid2D(float cellSize) :
		m_cellSize(cellSize),
		m_min(0.0f, 0.0f),
		m_width(0),
		m_height(0)
	{
	}

	void NavGrid2D::build(const PhysicsWorld2D& world, const b2Vec2& minCorner, const b2Vec2& maxCorner, float agentRadius, uint16 obstacleMask)
	{
		m_min = minCorner;
		m_width = std::max(1, static_cast<int>(std::ceil((maxCorner.x - minCorner.x) / m_cellSize)));
		m_height = std::max(1, static_cast<int>(std::ceil((maxCorner.y - minCorner.y) / m_cellSize)));
		m_walkable.assign(static_cast<size_t>(m_width) * m_height, 0);

		for (int y = 0; y < m_height; ++y)
		{
			for (int x = 0; x < m_width; ++x)
			{
				const b2Vec2 center = center_of(x, y);
				const bool free = !world.static_within(center, agentRadius, obstacleMask);
				m_walkable[static_cast<size_t>(y) * m_width + x] = free ? 1 : 0;
			}
		}
	}

	bool NavGrid2D::cell_of(const b2Vec2& point, int& x, int& y) const
	{
		x = static_cast<int>(std::floor((point.x - m_min.x) / m_cellSize));
		y = static_cast<int>(std::floor((point.y - m_min.y) / m_cellSize));
		return x >= 0 && y >= 0 && x < m_width && y < m_height;
	}

	b2Vec2 NavGrid2D::center_of(int x, int y) const
	{
		return b2Vec2(m_min.x + (x + 0.5f) * m_cellSize, m_min.y + (y + 0.5f) * m_cellSize);
	}

	bool NavGrid2D::walkable_cell(int x, int y) const
	{
		return x >= 0 && y >= 0 && x < m_width && y < m_height
			&& m_walkable[static_cast<size_t>(y) * m_width + x] != 0;
	}

	bool NavGrid2D::is_walkable(const b2Vec2& point) const
	{
		int x = 0, y = 0;
		return cell_of(point, x, y) && walkable_cell(x, y);
	}

	// Index of the walkable cell nearest to the point (within a few cells), or -1
	int NavGrid2D::nearest_walkable(const b2Vec2& point) const
	{
		int cx = 0, cy = 0;
		if (!cell_of(point, cx, cy))
		{
			return -1;
		}
		const int maxRing = 4;
		for (int ring = 0; ring <= maxRing; ++ring)
		{
			for (int dy = -ring; dy <= ring; ++dy)
			{
				for (int dx = -ring; dx <= ring; ++dx)
				{
					if (std::max(std::abs(dx), std::abs(dy)) != ring)
					{
						continue;
					}
					if (walkable_cell(cx + dx, cy + dy))
					{
						return (cy + dy) * m_width + (cx + dx);
					}
				}
			}
		}
		return -1;
	}

	bool NavGrid2D::find_path(const b2Vec2& from, const b2Vec2& to, std::vector<b2Vec2>& path) const
	{
		path.clear();
		const int start = nearest_walkable(from);
		const int goal = nearest_walkable(to);
		if (start < 0 || goal < 0)
		{
			return false;
		}

		const size_t count = static_cast<size_t>(m_width) * m_height;
		const float inf = std::numeric_limits<float>::max();
		std::vector<float> g(count, inf);
		std::vector<int> parent(count, -1);
		std::vector<uint8_t> closed(count, 0);

		auto heuristic = [this](int a, int b)
		{
			const float dx = static_cast<float>(std::abs(a % m_width - b % m_width));
			const float dy = static_cast<float>(std::abs(a / m_width - b / m_width));
			// octile distance
			return (dx + dy) + (std::sqrt(2.0f) - 2.0f) * std::min(dx, dy);
		};

		using Entry = std::pair<float, int>; // f, index
		std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
		g[start] = 0.0f;
		open.push({ heuristic(start, goal), start });

		const int dxs[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
		const int dys[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

		bool found = false;
		while (!open.empty())
		{
			const int current = open.top().second;
			open.pop();
			if (closed[current])
			{
				continue;
			}
			closed[current] = 1;
			if (current == goal)
			{
				found = true;
				break;
			}

			const int cx = current % m_width;
			const int cy = current / m_width;
			for (int k = 0; k < 8; ++k)
			{
				const int nx = cx + dxs[k];
				const int ny = cy + dys[k];
				if (!walkable_cell(nx, ny))
				{
					continue;
				}
				const bool diagonal = dxs[k] != 0 && dys[k] != 0;
				// Do not cut a blocked corner
				if (diagonal && (!walkable_cell(cx + dxs[k], cy) || !walkable_cell(cx, cy + dys[k])))
				{
					continue;
				}

				const int next = ny * m_width + nx;
				if (closed[next])
				{
					continue;
				}
				const float stepCost = diagonal ? std::sqrt(2.0f) : 1.0f;
				const float candidate = g[current] + stepCost;
				if (candidate < g[next])
				{
					g[next] = candidate;
					parent[next] = current;
					open.push({ candidate + heuristic(next, goal), next });
				}
			}
		}

		if (!found)
		{
			return false;
		}

		for (int node = goal; node != -1; node = parent[node])
		{
			path.push_back(center_of(node % m_width, node / m_width));
		}
		std::reverse(path.begin(), path.end());
		return true;
	}
}
