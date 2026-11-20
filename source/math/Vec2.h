///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Vec2.h
//
// Purpose: the engine's 2D vector, the one the 2D physics (PhysicsWorld2D, PhysicsBody2D, CharacterController2D) and
//          the games' simulations use. A game spells Pakal::Vec2, Pakal::dot and Pakal::cross, never a Box2D name
//          (docs/adr ADR-0017: games never see the backends).
//
//          It has the members and the arithmetic Box2D 2.4's b2Vec2 had, computed the same way (the same float
//          operations in the same order), so a simulation gives the same numbers it gave with Box2D's (a game's trained
//          brains, the deterministic bots). The physics converts at its boundary (source/box2D/Box2DInternal.h).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cfloat>
#include <cmath>

// The short integer names Box2D 2.4 declared (b2_types.h), which code of the engine and the games spells. The same
// declarations as Box2D's, so a file that sees both compiles.
typedef signed char int8;
typedef signed short int16;
typedef signed int int32;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;

namespace Pakal
{
	struct Vec2
	{
		float x, y;

		// (no initial value, like b2Vec2: a Vec2 declared alone holds garbage until set)
		Vec2() = default;
		constexpr Vec2(float xIn, float yIn) : x(xIn), y(yIn) {}

		void SetZero() { x = 0.0f; y = 0.0f; }
		void Set(float x_, float y_) { x = x_; y = y_; }

		Vec2 operator-() const { return Vec2(-x, -y); }
		// 0: x, 1: y
		float operator()(int i) const { return (&x)[i]; }
		float& operator()(int i) { return (&x)[i]; }

		void operator+=(const Vec2& v) { x += v.x; y += v.y; }
		void operator-=(const Vec2& v) { x -= v.x; y -= v.y; }
		void operator*=(float a) { x *= a; y *= a; }

		float Length() const { return std::sqrt(x * x + y * y); }
		float LengthSquared() const { return x * x + y * y; }
		// Makes it one long and returns the length it had; a vector shorter than FLT_EPSILON stays as it is and gives 0
		float Normalize()
		{
			const float length = Length();
			if (length < FLT_EPSILON)
			{
				return 0.0f;
			}
			const float invLength = 1.0f / length;
			x *= invLength;
			y *= invLength;
			return length;
		}
		bool IsValid() const { return std::isfinite(x) && std::isfinite(y); }
		// The vector turned a quarter to the left: (-y, x)
		Vec2 Skew() const { return Vec2(-y, x); }
	};

	inline Vec2 operator+(const Vec2& a, const Vec2& b) { return Vec2(a.x + b.x, a.y + b.y); }
	inline Vec2 operator-(const Vec2& a, const Vec2& b) { return Vec2(a.x - b.x, a.y - b.y); }
	inline Vec2 operator*(float s, const Vec2& a) { return Vec2(s * a.x, s * a.y); }
	inline Vec2 operator*(const Vec2& a, float s) { return Vec2(s * a.x, s * a.y); }
	inline bool operator==(const Vec2& a, const Vec2& b) { return a.x == b.x && a.y == b.y; }
	inline bool operator!=(const Vec2& a, const Vec2& b) { return a.x != b.x || a.y != b.y; }

	inline float dot(const Vec2& a, const Vec2& b) { return a.x * b.x + a.y * b.y; }
	// the z of the 3D cross product (a.x * b.y - a.y * b.x)
	inline float cross(const Vec2& a, const Vec2& b) { return a.x * b.y - a.y * b.x; }
	inline float distance(const Vec2& a, const Vec2& b) { return (a - b).Length(); }
	inline float distance_squared(const Vec2& a, const Vec2& b)
	{
		const Vec2 c = a - b;
		return dot(c, c);
	}

	// A box along the axes: lowerBound (its lower left corner) and upperBound (its upper right)
	struct Aabb2
	{
		Vec2 lowerBound;
		Vec2 upperBound;

		Vec2 GetCenter() const { return 0.5f * (lowerBound + upperBound); }
		Vec2 GetExtents() const { return 0.5f * (upperBound - lowerBound); }
	};
}
