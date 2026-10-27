///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: NavGrid2D.h
//
// Purpose: Walkable grid for top-down agents, built from the static geometry of a PhysicsWorld2D.
//          A cell is walkable when its center is at least agentRadius away from every static
//          surface. Paths are A* over 8 neighbours; diagonal steps may not cut a blocked corner.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Platform.h"
#include "box2D/PhysicsWorld2D.h"

#include <vector>

namespace Pakal
{
	class _PAKALExport NavGrid2D
	{
	public:
		// cellSize and coordinates are in meters
		explicit NavGrid2D(float cellSize);

		void build(const PhysicsWorld2D& world, const b2Vec2& minCorner, const b2Vec2& maxCorner, float agentRadius, uint16 obstacleMask = 0xFFFF);

		bool is_walkable(const b2Vec2& point) const;

		// Path of cell centers from 'from' to 'to' (both snapped to the nearest walkable cell).
		// Returns false if no path exists.
		bool find_path(const b2Vec2& from, const b2Vec2& to, std::vector<b2Vec2>& path) const;

		int get_width() const { return m_width; }
		int get_height() const { return m_height; }

	private:
		bool cell_of(const b2Vec2& point, int& x, int& y) const;
		b2Vec2 center_of(int x, int y) const;
		int nearest_walkable(const b2Vec2& point) const;
		bool walkable_cell(int x, int y) const;

		float m_cellSize;
		b2Vec2 m_min;
		int m_width;
		int m_height;
		std::vector<uint8_t> m_walkable;
	};
}
