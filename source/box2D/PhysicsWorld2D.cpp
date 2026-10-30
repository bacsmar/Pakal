///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PhysicsWorld2D.cpp
//
// Purpose: Top-down 2D physics world owned by the engine - implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "PhysicsWorld2D.h"

#include <box2d/b2_distance.h>

#include <algorithm>
#include <cfloat>

namespace Pakal
{
	PhysicsWorld2D::PhysicsWorld2D(const b2Vec2& gravity) :
		m_world(gravity)
	{
	}

	PhysicsWorld2D::~PhysicsWorld2D() = default;

	void PhysicsWorld2D::add_static_box(const b2Vec2& center, const b2Vec2& halfSize, uint16 categoryBits, uintptr_t userData, float angle)
	{
		b2BodyDef def;
		def.type = b2_staticBody;
		def.position = center;
		def.angle = angle;
		def.userData.pointer = userData;
		b2Body* body = m_world.CreateBody(&def);

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
		for (b2Body* body = m_world.GetBodyList(); body; body = body->GetNext())
		{
			if (body->GetType() == b2_staticBody && body->GetUserData().pointer == userData && body->IsEnabled() != enabled)
			{
				body->SetEnabled(enabled);
			}
		}
	}

	void PhysicsWorld2D::add_static_chain(const std::vector<b2Vec2>& points, bool closed, uint16 categoryBits)
	{
		if (points.size() < 2)
		{
			return;
		}
		b2BodyDef def;
		def.type = b2_staticBody;
		b2Body* body = m_world.CreateBody(&def);

		// One edge per segment: Box2D chains are one-sided at the ends, an edge per segment keeps every
		// segment solid from both sides
		const size_t count = closed ? points.size() : points.size() - 1;
		for (size_t i = 0; i < count; ++i)
		{
			const b2Vec2& a = points[i];
			const b2Vec2& b = points[(i + 1) % points.size()];
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

	void PhysicsWorld2D::add_static_circle(const b2Vec2& center, float radius, uint16 categoryBits, uintptr_t userData)
	{
		b2BodyDef def;
		def.type = b2_staticBody;
		def.position = center;
		def.userData.pointer = userData;
		b2Body* body = m_world.CreateBody(&def);

		b2CircleShape circle;
		circle.m_radius = radius;

		b2FixtureDef fixture;
		fixture.shape = &circle;
		fixture.filter.categoryBits = categoryBits;
		fixture.filter.maskBits = 0xFFFF;
		body->CreateFixture(&fixture);
	}

	void PhysicsWorld2D::add_static_polygon(const std::vector<b2Vec2>& points, uint16 categoryBits, uint16 maskBits)
	{
		if (points.size() < 3 || points.size() > b2_maxPolygonVertices) return;
		b2PolygonShape polygon;
		polygon.Set(points.data(), static_cast<int32>(points.size()));
		b2BodyDef def;
		b2Body* body = m_world.CreateBody(&def);
		b2FixtureDef fixture;
		fixture.shape = &polygon;
		fixture.filter.categoryBits = categoryBits;
		fixture.filter.maskBits = maskBits;
		body->CreateFixture(&fixture);
	}

	void PhysicsWorld2D::step(float dtSeconds)
	{
		m_world.Step(dtSeconds, 8, 3);
	}

	bool PhysicsWorld2D::point_in_static(const b2Vec2& point) const
	{
		for (const b2Body* body = m_world.GetBodyList(); body; body = body->GetNext())
		{
			if (body->GetType() != b2_staticBody)
			{
				continue;
			}
			for (const b2Fixture* fixture = body->GetFixtureList(); fixture; fixture = fixture->GetNext())
			{
				if (fixture->TestPoint(point))
				{
					return true;
				}
			}
		}
		return false;
	}

	float PhysicsWorld2D::distance_to_static(const b2Vec2& point, uint16 categoryMask) const
	{
		// A zero-radius circle at 'point' stands for the query point
		b2CircleShape probe;
		probe.m_radius = 0.0f;
		probe.m_p.SetZero();

		float closest = FLT_MAX;
		for (const b2Body* body = m_world.GetBodyList(); body; body = body->GetNext())
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
				input.transformB.p = point;
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

	bool PhysicsWorld2D::static_within(const b2Vec2& point, float radius, uint16 categoryMask) const
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
		query.point = point;
		query.radius = radius;
		query.mask = categoryMask;
		b2AABB box;
		box.lowerBound = point - b2Vec2(radius, radius);
		box.upperBound = point + b2Vec2(radius, radius);
		m_world.QueryAABB(&query, box);
		return query.found;
	}

	RayCastHit PhysicsWorld2D::raycast(const b2Vec2& from, const b2Vec2& to, uint16 categoryMask)
	{
		RayCastHit result = raycast_closest(&m_world, from, to, categoryMask);
		result.staticBody = result.hit && result.body && result.body->GetType() == b2_staticBody;
		return result;
	}
}
