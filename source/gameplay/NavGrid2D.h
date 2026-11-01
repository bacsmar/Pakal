///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: NavGrid2D.h
//
// Purpose: Walkable grid for top-down agents, built from the static geometry of a PhysicsWorld2D.
//          A cell is walkable when its center is at least agentRadius away from every static
//          surface. Paths are A* over 8 neighbours; diagonal steps may not cut a blocked corner.
//          The walkable cells are labelled by connected region when built: a goal is snapped to
//          the nearest cell of the start's region (the inside of a closed outline, such as the
//          sea's, is walkable but unreachable), and a path between regions fails at once.
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
		// Tests the cells of a box again against the world as it is now (something that blocked it broke, or appeared), with
		// the agent radius and obstacles of the last build, and connects the regions again
		void rebuild_area(const PhysicsWorld2D& world, const b2Vec2& minCorner, const b2Vec2& maxCorner);

		bool is_walkable(const b2Vec2& point) const;
		// The connected region of the walkable cell nearest the point (two cells round it), -1 when none: two points in
		// the same region have a walk between them (one in a pond shut by a chain of collision does not)
		int region_of(const b2Vec2& point) const;

		// Path of cell centers from 'from' to 'to' ('from' snapped to the nearest walkable cell, 'to' to the
		// nearest one 'from' can reach). Returns false if no path exists.
		bool find_path(const b2Vec2& from, const b2Vec2& to, std::vector<b2Vec2>& path) const;
		// Whether find_path would find one, without searching it (the regions): a question asked many times a second
		bool connected(const b2Vec2& from, const b2Vec2& to) const;

		int get_width() const { return m_width; }
		int get_height() const { return m_height; }
		// The box it covers and its cells (another grid can be built over the same box)
		b2Vec2 get_min_corner() const { return m_min; }
		b2Vec2 get_max_corner() const { return m_min + m_cellSize * b2Vec2(static_cast<float>(m_width), static_cast<float>(m_height)); }
		float get_agent_radius() const { return m_agentRadius; }

	private:
		bool cell_of(const b2Vec2& point, int& x, int& y) const;
		b2Vec2 center_of(int x, int y) const;
		// Index of the walkable cell nearest to the point within maxRing cells (of 'region' only when >= 0), or -1
		int nearest_walkable(const b2Vec2& point, int maxRing = 4, int region = -1) const;
		bool walkable_cell(int x, int y) const;
		void label_regions();

		float m_cellSize;
		b2Vec2 m_min;
		int m_width;
		int m_height;
		std::vector<uint8_t> m_walkable;
		std::vector<int> m_region;   // per cell: its connected region, -1 when blocked
		float m_agentRadius = 0.0f;   // the last build's
		uint16 m_obstacleMask = 0xFFFF;
	};
}
