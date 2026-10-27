///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CharacterController2D.cpp
//
// Purpose: Top-down character body for a Box2D world - implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "CharacterController2D.h"

namespace Pakal
{
	CharacterController2D::CharacterController2D(b2World* world, const b2Vec2& position, float radius,
		uint16 categoryBits, uint16 maskBits, uintptr_t userData) :
		m_world(world),
		m_body(nullptr),
		m_velocity(0.0f, 0.0f)
	{
		b2BodyDef bodyDef;
		bodyDef.type = b2_dynamicBody;
		bodyDef.position = position;
		bodyDef.fixedRotation = true;
		bodyDef.gravityScale = 0.0f; // top-down: no gravity
		bodyDef.linearDamping = 0.0f;
		m_body = m_world->CreateBody(&bodyDef);
		m_body->GetUserData().pointer = userData;

		b2CircleShape circle;
		circle.m_radius = radius;

		b2FixtureDef fixtureDef;
		fixtureDef.shape = &circle;
		fixtureDef.density = 1.0f;
		fixtureDef.friction = 0.0f; // walls should not drag the character
		fixtureDef.restitution = 0.0f;
		fixtureDef.filter.categoryBits = categoryBits;
		fixtureDef.filter.maskBits = maskBits;
		m_body->CreateFixture(&fixtureDef);
	}

	CharacterController2D::~CharacterController2D()
	{
		if (m_world && m_body)
		{
			m_world->DestroyBody(m_body);
		}
	}

	void CharacterController2D::set_move(const b2Vec2& direction, float speedMetersPerSecond)
	{
		b2Vec2 dir = direction;
		const float length = dir.Normalize();
		if (length > 0.0f)
		{
			m_velocity = b2Vec2(dir.x * speedMetersPerSecond, dir.y * speedMetersPerSecond);
		}
		else
		{
			m_velocity = b2Vec2(0.0f, 0.0f);
		}
	}

	void CharacterController2D::update()
	{
		m_body->SetLinearVelocity(m_velocity);
	}

	b2Vec2 CharacterController2D::get_position() const
	{
		return m_body->GetPosition();
	}

	void CharacterController2D::teleport(const b2Vec2& position)
	{
		m_body->SetTransform(position, 0.0f);
		m_body->SetLinearVelocity(b2Vec2(0.0f, 0.0f));
		m_velocity.Set(0.0f, 0.0f);
	}

	void CharacterController2D::set_enabled(bool enabled)
	{
		m_body->SetEnabled(enabled);
	}
}
