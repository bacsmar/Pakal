///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CharacterController2D.cpp
//
// Purpose: Top-down character body for a PhysicsWorld2D - implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "CharacterController2D.h"
#include "Box2DInternal.h"

namespace Pakal
{
	CharacterController2D::CharacterController2D(PhysicsWorld2D& world, const Vec2& position, float radius,
		uint16 categoryBits, uint16 maskBits, uintptr_t userData) :
		m_world(world),
		m_body(nullptr),
		m_velocity(0.0f, 0.0f)
	{
		b2BodyDef bodyDef;
		bodyDef.type = b2_dynamicBody;
		bodyDef.position = to_b2(position);
		bodyDef.fixedRotation = true;
		bodyDef.gravityScale = 0.0f; // top-down: no gravity
		bodyDef.linearDamping = 0.0f;
		b2Body* body = box2d_world(m_world).CreateBody(&bodyDef);
		m_body = body;
		body->GetUserData().pointer = userData;

		b2CircleShape circle;
		circle.m_radius = radius;

		b2FixtureDef fixtureDef;
		fixtureDef.shape = &circle;
		fixtureDef.density = 1.0f;
		fixtureDef.friction = 0.0f; // walls should not drag the character
		fixtureDef.restitution = 0.0f;
		fixtureDef.filter.categoryBits = categoryBits;
		fixtureDef.filter.maskBits = maskBits;
		body->CreateFixture(&fixtureDef);
	}

	CharacterController2D::~CharacterController2D()
	{
		if (m_body)
		{
			box2d_world(m_world).DestroyBody(box2d_body(m_body));
		}
	}

	void CharacterController2D::set_move(const Vec2& direction, float speedMetersPerSecond)
	{
		Vec2 dir = direction;
		const float length = dir.Normalize();
		if (length > 0.0f)
		{
			m_velocity = Vec2(dir.x * speedMetersPerSecond, dir.y * speedMetersPerSecond);
		}
		else
		{
			m_velocity = Vec2(0.0f, 0.0f);
		}
	}

	void CharacterController2D::update()
	{
		box2d_body(m_body)->SetLinearVelocity(to_b2(m_velocity));
	}

	Vec2 CharacterController2D::get_position() const
	{
		return to_vec2(box2d_body(m_body)->GetPosition());
	}

	void CharacterController2D::teleport(const Vec2& position)
	{
		box2d_body(m_body)->SetTransform(to_b2(position), 0.0f);
		box2d_body(m_body)->SetLinearVelocity(b2Vec2(0.0f, 0.0f));
		m_velocity.Set(0.0f, 0.0f);
	}

	void CharacterController2D::set_enabled(bool enabled)
	{
		box2d_body(m_body)->SetEnabled(enabled);
	}

	void CharacterController2D::set_mask(uint16 maskBits)
	{
		for (b2Fixture* fixture = box2d_body(m_body)->GetFixtureList(); fixture; fixture = fixture->GetNext())
		{
			b2Filter filter = fixture->GetFilterData();
			if (filter.maskBits == maskBits) continue;
			filter.maskBits = maskBits;
			fixture->SetFilterData(filter);
		}
	}
}
