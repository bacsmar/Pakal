///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PhysicsWorld2D.cpp
//
// Purpose: Top-down 2D physics world owned by the engine - implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "PhysicsWorld2D.h"
#include "Box2DInternal.h"

#include <box2d/b2_distance.h>

#include <algorithm>
#include <cfloat>

namespace Pakal
{
	PhysicsWorld2D::PhysicsWorld2D(const Vec2& gravity) :
		m_impl(std::make_unique<Impl>(to_b2(gravity)))
	{
	}

	PhysicsWorld2D::~PhysicsWorld2D() = default;

	void PhysicsWorld2D::add_static_box(const Vec2& center, const Vec2& halfSize, uint16 categoryBits, uintptr_t userData, float angle)
	{
		b2BodyDef def;
		def.type = b2_staticBody;
		def.position = to_b2(center);
		def.angle = angle;
		def.userData.pointer = userData;
		b2Body* body = m_impl->world.CreateBody(&def);

		b2PolygonShape box;
		box.SetAsBox(halfSize.x, halfSize.y);

		b2FixtureDef fixture;
		fixture.shape = &box;
		fixture.filter.categoryBits = categoryBits;
		fixture.filter.maskBits = 0xFFFF;
		body->CreateFixture(&fixture);
	}

	void PhysicsWorld2D::set_static_enabled(uintptr_t userData, bool enabled)
	{
		if (userData == 0)
		{
			return;
		}
		for (b2Body* body = m_impl->world.GetBodyList(); body; body = body->GetNext())
		{
			if (body->GetType() == b2_staticBody && body->GetUserData().pointer == userData && body->IsEnabled() != enabled)
			{
				body->SetEnabled(enabled);
			}
		}
	}

	void PhysicsWorld2D::add_static_chain(const std::vector<Vec2>& points, bool closed, uint16 categoryBits)
	{
		if (points.size() < 2)
		{
			return;
		}
		b2BodyDef def;
		def.type = b2_staticBody;
		b2Body* body = m_impl->world.CreateBody(&def);

		// One edge per segment: Box2D chains are one-sided at the ends, an edge per segment keeps every
		// segment solid from both sides
		const size_t count = closed ? points.size() : points.size() - 1;
		for (size_t i = 0; i < count; ++i)
		{
			const b2Vec2 a = to_b2(points[i]);
			const b2Vec2 b = to_b2(points[(i + 1) % points.size()]);
			if ((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y) < 1e-8f)
			{
				continue; // degenerate segment
			}
			b2EdgeShape edge;
			edge.SetTwoSided(a, b);

			b2FixtureDef fixture;
			fixture.shape = &edge;
			fixture.filter.categoryBits = categoryBits;
			fixture.filter.maskBits = 0xFFFF;
			body->CreateFixture(&fixture);
		}
	}

	void PhysicsWorld2D::add_static_circle(const Vec2& center, float radius, uint16 categoryBits, uintptr_t userData)
	{
		b2BodyDef def;
		def.type = b2_staticBody;
		def.position = to_b2(center);
		def.userData.pointer = userData;
		b2Body* body = m_impl->world.CreateBody(&def);

		b2CircleShape circle;
		circle.m_radius = radius;

		b2FixtureDef fixture;
		fixture.shape = &circle;
		fixture.filter.categoryBits = categoryBits;
		fixture.filter.maskBits = 0xFFFF;
		body->CreateFixture(&fixture);
	}

	void PhysicsWorld2D::add_static_polygon(const std::vector<Vec2>& points, uint16 categoryBits, uint16 maskBits)
	{
		if (points.size() < 3 || points.size() > b2_maxPolygonVertices) return;
		b2Vec2 corners[b2_maxPolygonVertices];
		for (size_t i = 0; i < points.size(); ++i) corners[i] = to_b2(points[i]);
		b2PolygonShape polygon;
		polygon.Set(corners, static_cast<int32>(points.size()));
		b2BodyDef def;
		b2Body* body = m_impl->world.CreateBody(&def);
		b2FixtureDef fixture;
		fixture.shape = &polygon;
		fixture.filter.categoryBits = categoryBits;
		fixture.filter.maskBits = maskBits;
		body->CreateFixture(&fixture);
	}

	void PhysicsWorld2D::step(float dtSeconds)
	{
		m_impl->world.Step(dtSeconds, 8, 3);
	}

	bool PhysicsWorld2D::point_in_static(const Vec2& point) const
	{
		for (const b2Body* body = m_impl->world.GetBodyList(); body; body = body->GetNext())
		{
			if (body->GetType() != b2_staticBody)
			{
				continue;
			}
			for (const b2Fixture* fixture = body->GetFixtureList(); fixture; fixture = fixture->GetNext())
			{
				if (fixture->TestPoint(to_b2(point)))
				{
					return true;
				}
			}
		}
		return false;
	}

	float PhysicsWorld2D::distance_to_static(const Vec2& point, uint16 categoryMask) const
	{
		// A zero-radius circle at 'point' stands for the query point
		b2CircleShape probe;
		probe.m_radius = 0.0f;
		probe.m_p.SetZero();

		float closest = FLT_MAX;
		for (const b2Body* body = m_impl->world.GetBodyList(); body; body = body->GetNext())
		{
			if (body->GetType() != b2_staticBody)
			{
				continue;
			}
			for (const b2Fixture* fixture = body->GetFixtureList(); fixture; fixture = fixture->GetNext())
			{
				b2DistanceInput input;
				if ((fixture->GetFilterData().categoryBits & categoryMask) == 0) continue;
				input.proxyA.Set(fixture->GetShape(), 0);
				input.proxyB.Set(&probe, 0);
				input.transformA = body->GetTransform();
				input.transformB.p = to_b2(point);
				input.transformB.q.SetIdentity();
				input.useRadii = true;

				b2SimplexCache cache;
				cache.count = 0;
				b2DistanceOutput output;
				b2Distance(&output, &cache, &input);
				closest = std::min(closest, output.distance);
			}
		}
		return closest;
	}

	bool PhysicsWorld2D::static_within(const Vec2& point, float radius, uint16 categoryMask) const
	{
		struct Query final : b2QueryCallback
		{
			b2Vec2 point;
			float radius = 0.0f;
			uint16 mask = 0xFFFF;
			bool found = false;
			bool ReportFixture(b2Fixture* fixture) override
			{
				const b2Body* body = fixture->GetBody();
				if (body->GetType() != b2_staticBody || (fixture->GetFilterData().categoryBits & mask) == 0) return true;
				b2CircleShape probe;
				probe.m_radius = 0.0f;
				probe.m_p.SetZero();
				// every child: a chain is one edge per child
				const b2Shape* shape = fixture->GetShape();
				for (int32 child = 0; child < shape->GetChildCount(); ++child)
				{
					b2DistanceInput input;
					input.proxyA.Set(shape, child);
					input.proxyB.Set(&probe, 0);
					input.transformA = body->GetTransform();
					input.transformB.p = point;
					input.transformB.q.SetIdentity();
					input.useRadii = true;
					b2SimplexCache cache;
					cache.count = 0;
					b2DistanceOutput output;
					b2Distance(&output, &cache, &input);
					if (output.distance < radius)
					{
						found = true;
						return false;
					}
				}
				return true;
			}
		} query;
		query.point = to_b2(point);
		query.radius = radius;
		query.mask = categoryMask;
		b2AABB box;
		box.lowerBound = to_b2(point) - b2Vec2(radius, radius);
		box.upperBound = to_b2(point) + b2Vec2(radius, radius);
		m_impl->world.QueryAABB(&query, box);
		return query.found;
	}

	RayCastHit PhysicsWorld2D::raycast(const Vec2& from, const Vec2& to, uint16 categoryMask)
	{
		const b2Body* body = nullptr;
		RayCastHit result = raycast_closest(m_impl->world, to_b2(from), to_b2(to), categoryMask, &body);
		result.staticBody = result.hit && body && body->GetType() == b2_staticBody;
		return result;
	}

	void PhysicsWorld2D::for_each_shape(const std::function<void(const PhysicsShape2D&)>& visit) const
	{
		std::vector<Vec2> points;
		for (const b2Body* body = m_impl->world.GetBodyList(); body; body = body->GetNext())
		{
			const b2Transform& xf = body->GetTransform();
			for (const b2Fixture* fixture = body->GetFixtureList(); fixture; fixture = fixture->GetNext())
			{
				const b2Shape* shape = fixture->GetShape();
				PhysicsShape2D out;
				out.isStatic = body->GetType() == b2_staticBody;
				out.enabled = body->IsEnabled();
				out.categoryBits = fixture->GetFilterData().categoryBits;
				out.userData = const_cast<b2Body*>(body)->GetUserData().pointer;   // (Box2D 2.4 has no const one)
				// the box around the whole shape (a chain has one per link)
				bool first = true;
				for (int32 child = 0; child < shape->GetChildCount(); ++child)
				{
					b2AABB box;
					shape->ComputeAABB(&box, xf, child);
					out.lower = to_vec2(first ? box.lowerBound : b2Min(to_b2(out.lower), box.lowerBound));
					out.upper = to_vec2(first ? box.upperBound : b2Max(to_b2(out.upper), box.upperBound));
					first = false;
				}
				points.clear();
				switch (shape->GetType())
				{
				case b2Shape::e_polygon:
				{
					const auto* polygon = static_cast<const b2PolygonShape*>(shape);
					out.kind = PhysicsShape2D::Kind::Polygon;
					for (int32 i = 0; i < polygon->m_count; ++i) points.push_back(to_vec2(b2Mul(xf, polygon->m_vertices[i])));
					break;
				}
				case b2Shape::e_circle:
				{
					const auto* circle = static_cast<const b2CircleShape*>(shape);
					out.kind = PhysicsShape2D::Kind::Circle;
					out.center = to_vec2(b2Mul(xf, circle->m_p));
					out.radius = circle->m_radius;
					break;
				}
				case b2Shape::e_chain:
				{
					const auto* chain = static_cast<const b2ChainShape*>(shape);
					out.kind = PhysicsShape2D::Kind::Chain;
					for (int32 i = 0; i < chain->m_count; ++i) points.push_back(to_vec2(b2Mul(xf, chain->m_vertices[i])));
					break;
				}
				case b2Shape::e_edge:
				{
					const auto* edge = static_cast<const b2EdgeShape*>(shape);
					out.kind = PhysicsShape2D::Kind::Edge;
					points.push_back(to_vec2(b2Mul(xf, edge->m_vertex1)));
					points.push_back(to_vec2(b2Mul(xf, edge->m_vertex2)));
					break;
				}
				default:
					continue;
				}
				out.points = points.empty() ? nullptr : points.data();
				out.count = static_cast<int>(points.size());
				visit(out);
			}
		}
	}
}
