///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Height.h
// Purpose: The vertical axis over Box2D's plane: a character with a HeightComponent can leave the
//          floor (a jump, a launch from a hit) and falls back under gravity. Box2D still moves it on
//          the floor's plane; the height is kinematic, stepped here. Hits can ask whether two actors
//          are at heights that meet (heights_meet), and the view draws the character that high.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/Actor.h"

#include <algorithm>
#include <cmath>

namespace Pakal::TopDown
{
	class HeightComponent : public Component
	{
	public:
		float height = 0.0f;           // meters above the floor
		float verticalSpeed = 0.0f;    // meters per second, up
		float gravity = 20.0f;         // meters per second squared
		float tall = 1.8f;             // how tall it is (meters): what a hit at a height can reach

		// Leaves the floor at this speed (only from the floor, unless 'inAir')
		bool jump(float speed, bool inAir = false)
		{
			if (airborne() && !inAir) return false;
			verticalSpeed = speed;
			return true;
		}
		bool airborne() const { return height > 0.0f || verticalSpeed > 0.0f; }
		// Where to draw it 'alpha' of the way from the last step to this one
		float drawn_height(float alpha) const { return m_previous + alpha * (height - m_previous); }

		void update(TopDownGame& game, float dt) override
		{
			(void)game;
			m_previous = height;
			if (!airborne()) return;
			verticalSpeed -= gravity * dt;
			height += verticalSpeed * dt;
			if (height <= 0.0f)
			{
				height = 0.0f;
				verticalSpeed = 0.0f;   // landed
			}
		}

	private:
		float m_previous = 0.0f;
	};

	// The height of an actor's feet (0: on the floor, or it has no HeightComponent) and how tall it is
	inline float height_of(const Actor& actor)
	{
		const HeightComponent* height = actor.get<HeightComponent>();
		return height ? height->height : 0.0f;
	}

	// Whether something at 'height' meters over the floor (a bullet) passes through the body of 'target' (its feet to its top)
	inline bool body_at(const Actor& target, float height)
	{
		const HeightComponent* body = target.get<HeightComponent>();
		const float bottom = body ? body->height : 0.0f;
		return height >= bottom && height <= bottom + (body ? body->tall : 1.8f);
	}

	// Whether a hit 'reach' meters tall, from the feet of 'attacker', meets the body of 'target': their spans overlap
	inline bool heights_meet(const Actor& attacker, const Actor& target, float reach)
	{
		const float from = height_of(attacker);
		const HeightComponent* body = target.get<HeightComponent>();
		const float bottom = body ? body->height : 0.0f;
		const float top = bottom + (body ? body->tall : 1.8f);
		return from <= top && from + reach >= bottom;
	}
}
