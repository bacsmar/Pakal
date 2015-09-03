#pragma once

#include "PhysicsSystem.h"
#include <Box2D.h>

#include "IDebugDrawer.h"
#include <mutex>

#ifdef PAKAL_WIN32_PLATFORM	
	#pragma comment(lib, "Box2D.lib")
#endif

namespace Pakal
{	
	class ContactListener;
	class ContactFilter;
	class DestructionListener;
	class Component;
	//////////////////////////////////////////////////////////////////////////	

	class _PAKALExport Box2DPhysicsSystem final : public PhysicsSystem, IDebugDrawerClient
	{
	public:	
		virtual ~Box2DPhysicsSystem() {};
		explicit Box2DPhysicsSystem(bool usesThread);

		inline b2World* getWorld() const
		{
			return m_pWorld;
		}

		inline const char* get_system_name() override final
		{
			return "Box2DPhysicsSystem";
		}

		b2Body	* create_body(const b2BodyDef* def);
		b2Joint * create_joint(const b2JointDef* def);
		void destroy_body(b2Body* body);
		void destroy_joint(b2Joint* joint);
	protected:

		friend class Engine;

		// from IComponentProvider
		virtual void		register_component_factories( std::vector<IComponentFactory*> &factories) override;		
		
		// from IDebugDrawinfo
		virtual IDebugDrawerClient * get_debug_drawer() override { return this; };		
		virtual void do_debug_draw() override;
		virtual void set_drawer(const RendererInfo *renderInfo) override;

		// from PhysicsSystem
		virtual void update_world(long long dt) override;
		virtual void init_world() override;
		virtual void clear_world() override;
		
		inline void enable();
		inline void disable();		

	private:
		b2World				* m_pWorld;
		ContactListener		* m_pContactListener;
		ContactFilter		* m_pContactFilter;
		DestructionListener	* m_pDestructionListener;
		b2Draw				* m_pDebugDraw;

		std::mutex			m_debugDrawMutex;

		std::vector<std::pair<b2Body*, bool> > m_EnableQueue;
	};
}