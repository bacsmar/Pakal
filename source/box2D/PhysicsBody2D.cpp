///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PhysicsBody2D.cpp
// Purpose: A box-shaped body of a PhysicsWorld2D with gravity - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "box2D/PhysicsBody2D.h"
#include "box2D/Box2DInternal.h"

namespace Pakal
{
	PhysicsBody2D::PhysicsBody2D(PhysicsWorld2D& world, const Def& def) : m_world(world), m_body(nullptr)
	{
		b2BodyDef bodyDef;
		bodyDef.type = def.type == Type::Static ? b2_staticBody : def.type == Type::Kinematic ? b2_kinematicBody : b2_dynamicBody;
		bodyDef.position = to_b2(def.position);
		bodyDef.fixedRotation = def.fixedRotation;
		bodyDef.gravityScale = def.gravityScale;
		b2Body* body = box2d_world(m_world).CreateBody(&bodyDef);
		m_body = body;

		b2PolygonShape box;
		box.SetAsBox(def.halfSize.x, def.halfSize.y, to_b2(def.offset), 0.0f);
		b2FixtureDef fixture;
		fixture.shape = &box;
		fixture.density = def.density;
		fixture.friction = def.friction;
		fixture.restitution = def.restitution;
		fixture.isSensor = def.sensor;
		fixture.filter.categoryBits = def.categoryBits;
		fixture.filter.maskBits = def.maskBits;
		body->CreateFixture(&fixture);
	}

	PhysicsBody2D::~PhysicsBody2D()
	{
		if (m_body) box2d_world(m_world).DestroyBody(box2d_body(m_body));
	}

	Vec2 PhysicsBody2D::position() const { return to_vec2(box2d_body(m_body)->GetPosition()); }
	float PhysicsBody2D::angle() const { return box2d_body(m_body)->GetAngle(); }
	Vec2 PhysicsBody2D::velocity() const { return to_vec2(box2d_body(m_body)->GetLinearVelocity()); }
	float PhysicsBody2D::mass() const { return box2d_body(m_body)->GetMass(); }
	void PhysicsBody2D::set_position(const Vec2& position) { box2d_body(m_body)->SetTransform(to_b2(position), box2d_body(m_body)->GetAngle()); }
	void PhysicsBody2D::set_velocity(const Vec2& velocity) { box2d_body(m_body)->SetLinearVelocity(to_b2(velocity)); }
	void PhysicsBody2D::apply_impulse(const Vec2& impulse) { box2d_body(m_body)->ApplyLinearImpulseToCenter(to_b2(impulse), true); }
	void PhysicsBody2D::set_gravity_scale(float scale) { box2d_body(m_body)->SetGravityScale(scale); }
	void PhysicsBody2D::set_enabled(bool enabled) { box2d_body(m_body)->SetEnabled(enabled); }

	void PhysicsBody2D::bounds(Vec2& lower, Vec2& upper) const
	{
		const b2AABB box = box2d_body(m_body)->GetFixtureList()->GetAABB(0);
		lower = to_vec2(box.lowerBound);
		upper = to_vec2(box.upperBound);
	}

	bool PhysicsBody2D::supported() const
	{
		const b2Body* self = box2d_body(m_body);
		for (const b2ContactEdge* edge = self->GetContactList(); edge; edge = edge->next)
		{
			b2Contact* contact = edge->contact;
			if (!contact->IsTouching() || contact->GetFixtureA()->IsSensor() || contact->GetFixtureB()->IsSensor()) continue;
			b2WorldManifold manifold;
			contact->GetWorldManifold(&manifold);
			// the normal points from fixture A to fixture B
			const float up = contact->GetFixtureA()->GetBody() == self ? -manifold.normal.y : manifold.normal.y;
			if (up > 0.5f) return true;
		}
		return false;
	}

	bool PhysicsBody2D::touching(const PhysicsBody2D& other) const
	{
		for (const b2ContactEdge* edge = box2d_body(m_body)->GetContactList(); edge; edge = edge->next)
			if (edge->other == other.m_body && edge->contact->IsTouching()) return true;
		return false;
	}

	bool PhysicsBody2D::overlaps(const PhysicsBody2D& other) const
	{
		return b2TestOverlap(box2d_body(m_body)->GetFixtureList()->GetAABB(0), box2d_body(other.m_body)->GetFixtureList()->GetAABB(0));
	}
}
