///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Box2DQueries.cpp
//
// Purpose: World queries for Box2D worlds - implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Box2DQueries.h"

namespace Pakal
{
	namespace
	{
		class ClosestRayCast final : public b2RayCastCallback
		{
		public:
			explicit ClosestRayCast(uint16 categoryMask) : m_mask(categoryMask) {}

			float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float fraction) override
			{
				if ((fixture->GetFilterData().categoryBits & m_mask) == 0)
				{
					return -1.0f; // ignore this fixture and keep going
				}

				m_result.hit = true;
				m_result.point = point;
				m_result.normal = normal;
				m_result.fraction = fraction;
				m_result.body = fixture->GetBody();
				m_result.categoryBits = fixture->GetFilterData().categoryBits;
				m_result.userData = m_result.body->GetUserData().pointer;
				// Clip the ray to this hit: later reports can only be closer
				return fraction;
			}

			RayCastHit result() const { return m_result; }

		private:
			uint16 m_mask;
			RayCastHit m_result;
		};
	}

	RayCastHit raycast_closest(b2World* world, const b2Vec2& from, const b2Vec2& to, uint16 categoryMask)
	{
		if (!world)
		{
			return RayCastHit();
		}

		ClosestRayCast callback(categoryMask);
		world->RayCast(&callback, from, to);
		return callback.result();
	}
}
