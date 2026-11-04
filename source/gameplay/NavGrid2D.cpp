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
		m_agentRadius = agentRadius;
		m_obstacleMask = obstacleMask;

		for (int y = 0; y < m_height; ++y)
		{
			for (int x = 0; x < m_width; ++x)
			{
				const b2Vec2 center = center_of(x, y);
				const bool free = !world.static_within(center, agentRadius, obstacleMask);
				m_walkable[static_cast<size_t>(y) * m_width + x] = free ? 1 : 0;
			}
		}
		label_regions();
	}

	void NavGrid2D::rebuild_area(const PhysicsWorld2D& world, const b2Vec2& minCorner, const b2Vec2& maxCorner)
	{
		if (m_walkable.empty()) return;
		int x0, y0, x1, y1;
		cell_of(minCorner, x0, y0);
		cell_of(maxCorner, x1, y1);
		x0 = std::clamp(x0, 0, m_width - 1);
		x1 = std::clamp(x1, 0, m_width - 1);
		y0 = std::clamp(y0, 0, m_height - 1);
		y1 = std::clamp(y1, 0, m_height - 1);
		for (int y = y0; y <= y1; ++y)
		{
			for (int x = x0; x <= x1; ++x)
			{
				m_walkable[static_cast<size_t>(y) * m_width + x] = world.static_within(center_of(x, y), m_agentRadius, m_obstacleMask) ? 0 : 1;
			}
		}
		label_regions();
	}

	// Flood fill over the 4 neighbours (a diagonal step needs both of its sides free, so they connect the same cells)
	void NavGrid2D::label_regions()
	{
		m_region.assign(m_walkable.size(), -1);
		std::vector<int> stack;
		int next = 0;
		for (int seed = 0; seed < static_cast<int>(m_walkable.size()); ++seed)
		{
			if (!m_walkable[seed] || m_region[seed] >= 0)
			{
				continue;
			}
			m_region[seed] = next;
			stack.push_back(seed);
			while (!stack.empty())
			{
				const int cell = stack.back();
				stack.pop_back();
				const int cx = cell % m_width;
				const int cy = cell / m_width;
				const int around[4][2] = { { cx + 1, cy }, { cx - 1, cy }, { cx, cy + 1 }, { cx, cy - 1 } };
				for (const auto& n : around)
				{
					const int index = n[1] * m_width + n[0];
					if (walkable_cell(n[0], n[1]) && m_region[index] < 0)
					{
						m_region[index] = next;
						stack.push_back(index);
					}
				}
			}
			++next;
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

	int NavGrid2D::region_of(const b2Vec2& point) const
	{
		const int cell = nearest_walkable(point, 2);
		return cell < 0 ? -1 : m_region[static_cast<size_t>(cell)];
	}

	int NavGrid2D::nearest_walkable(const b2Vec2& point, int maxRing, int region) const
	{
		int cx = 0, cy = 0;
		if (!cell_of(point, cx, cy))
		{
			return -1;
		}
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
					const int index = (cy + dy) * m_width + (cx + dx);
					if (walkable_cell(cx + dx, cy + dy) && (region < 0 || m_region[index] == region))
					{
						return index;
					}
				}
			}
		}
		return -1;
	}

	bool NavGrid2D::connected(const b2Vec2& from, const b2Vec2& to) const
	{
		const int start = nearest_walkable(from);
		return start >= 0 && nearest_walkable(to, 16, m_region[start]) >= 0;
	}

	bool NavGrid2D::find_path(const b2Vec2& from, const b2Vec2& to, std::vector<b2Vec2>& path) const
	{
		path.clear();
		const int start = nearest_walkable(from);
		// the goal where the start's region comes nearest to it (a target on a shore: the land beside it, not the sea's
		// walkable but closed inside); none near: no path, without searching every cell the start reaches
		const int goal = start < 0 ? -1 : nearest_walkable(to, 16, m_region[start]);
		if (start < 0 || goal < 0)
		{
			return false;
		}

		// The search's cells, kept between searches (per thread): a cell holds this search's values only when its stamp is this
		// search's, so nothing is cleared (a town's grid is a million cells: clearing them was most of a search's cost, and
		// a game's agents search many times a second)
		struct Scratch
		{
			std::vector<float> g;
			std::vector<int> parent;
			std::vector<uint32_t> seen;     // the search that last touched the cell
			std::vector<uint32_t> closed;   // the search that closed it
			uint32_t search = 0;
		};
		thread_local Scratch scratch;
		const size_t count = static_cast<size_t>(m_width) * m_height;
		if (scratch.seen.size() < count || ++scratch.search == 0)
		{
			scratch.g.assign(std::max(count, scratch.g.size()), 0.0f);
			scratch.parent.assign(scratch.g.size(), -1);
			scratch.seen.assign(scratch.g.size(), 0);
			scratch.closed.assign(scratch.g.size(), 0);
			scratch.search = 1;
		}
		const uint32_t search = scratch.search;
		const float inf = std::numeric_limits<float>::max();
		std::vector<float>& gs = scratch.g;
		std::vector<int>& parent = scratch.parent;
		auto g = [&](int cell) { return scratch.seen[cell] == search ? gs[cell] : inf; };
		auto set_g = [&](int cell, float value, int from) { scratch.seen[cell] = search; gs[cell] = value; parent[cell] = from; };
		auto closed = [&](int cell) { return scratch.closed[cell] == search; };

		auto heuristic = [this](int a, int b)
		{
			const float dx = static_cast<float>(std::abs(a % m_width - b % m_width));
			const float dy = static_cast<float>(std::abs(a / m_width - b / m_width));
			// octile distance
			return (dx + dy) + (std::sqrt(2.0f) - 2.0f) * std::min(dx, dy);
		};

		using Entry = std::pair<float, int>; // f, index
		std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;
		set_g(start, 0.0f, -1);
		open.push({ heuristic(start, goal), start });

		const int dxs[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
		const int dys[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

		bool found = false;
		while (!open.empty())
		{
			const int current = open.top().second;
			open.pop();
			if (closed(current))
			{
				continue;
			}
			scratch.closed[current] = search;
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
				if (closed(next))
				{
					continue;
				}
				const float stepCost = diagonal ? std::sqrt(2.0f) : 1.0f;
				const float candidate = g(current) + stepCost;
				if (candidate < g(next))
				{
					set_g(next, candidate, current);
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
