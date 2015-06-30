#pragma once

#include <Box2D.h>
#include <vector>

namespace Pakal
{	
	//////////////////////////////////////////////////////////////////////////	
	enum ECollisionBitFlags
	{
		k_CollisionBitFlags_None			= 0,
	};
	//////////////////////////////////////////////////////////////////////////
	class ContactListener : public b2ContactListener
	{
	public:
		ContactListener() : m_bDisabled(false) { }

		// TODO: implement this contact Listener
		virtual void BeginContact(b2Contact* contact) override	{};
		virtual void EndContact(b2Contact* contact) override	{};
		virtual void PreSolve(b2Contact* contact, const b2Manifold* oldManifold) override		{};
		virtual void PostSolve(b2Contact* contact, const b2ContactImpulse* impulse) override	{};

		void Enable()
		{
			m_bDisabled = false;
		}

		void Disable()
		{
			m_bDisabled = true;
		}

	protected:
		bool m_bDisabled;
		std::vector<std::pair<b2Body*, b2MassData> > m_resetList;
	};
	//////////////////////////////////////////////////////////////////////////
	class ContactFilter : public b2ContactFilter
	{
	public:
		// TODO: implement this
		virtual bool ShouldCollide(b2Fixture* fixtureA, b2Fixture* fixtureB) { return false; } 
	};
	//////////////////////////////////////////////////////////////////////////
	class DestructionListener : public b2DestructionListener
	{
	public:

		virtual void SayGoodbye(b2Joint* joint) {};
		virtual void SayGoodbye(b2Fixture* fixture) {};
	};
	//////////////////////////////////////////////////////////////////////////		
}