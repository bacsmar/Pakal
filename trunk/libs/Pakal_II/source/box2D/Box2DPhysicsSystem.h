#pragma once

#include "PhysicsSystem.h"
#include <Box2D.h>

#ifdef PAKAL_WIN32_PLATFORM	
	#pragma comment(lib, "Box2D.lib")
#endif

namespace Pakal
{
	class Engine;

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

	class _PAKALExport Box2DPhysicsSystem : public PhysicsSystem
	{
	public:	
		virtual ~Box2DPhysicsSystem();
		Box2DPhysicsSystem();
	protected:

		friend class Engine;		

		virtual void registerComponentFactories( std::vector<IComponentFactory*> &factories) override;
		virtual void update() override;
		virtual void initWorld() override;
		virtual void clearWorld() override;
		
		inline void enable()	{ m_pContactListener->Enable(); }
		inline void disable()	{ m_pContactListener->Disable(); }

		inline b2World* getWorld() const
		{
			return m_pWorld;
		}

		b2Body * createBody(const b2BodyDef* def);
		b2Joint * createJoint(const b2JointDef* def);
		void destroyBody(b2Body* body);
		void destroyJoint(b2Joint* joint);

	private:
		b2World				* m_pWorld;
		ContactListener	* m_pContactListener;
		ContactFilter		* m_pContactFilter;
		DestructionListener* m_pDestructionListener;
		b2Draw				*m_pDebugDraw;

		std::vector<std::pair<b2Body*, bool> > m_EnableQueue;
	};
}