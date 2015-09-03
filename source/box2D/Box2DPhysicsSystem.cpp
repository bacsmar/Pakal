#include "Config.h"
#include "Box2DPhysicsSystem.h"
#include "Box2DPhysicsListeners.h"

#include "ComponentFactory.h"
#include "components/BodyComponent.h"
#include "Components/BodyComponent_Box2D.h"

#if PAKAL_USE_IRRLICHT
	#include "box2d/B2DebugDrawIrr.h"	
#endif


using namespace Pakal;

//////////////////////////////////////////////////////////////////////////
Box2DPhysicsSystem::Box2DPhysicsSystem(bool usesThread) :
		PhysicsSystem(usesThread),
		m_pWorld(nullptr),
		m_pContactListener(nullptr),
		m_pContactFilter(nullptr),
		m_pDestructionListener(nullptr),
		m_pDebugDraw(nullptr)		
{	
}
//////////////////////////////////////////////////////////////////////////
void Box2DPhysicsSystem::register_component_factories( std::vector<IComponentFactory*> &factories )
{	
	factories.push_back( CreateComponentFactory<BodyComponent,BodyComponent_Box2D>(this) );
}
//////////////////////////////////////////////////////////////////////////
b2Body* Box2DPhysicsSystem::create_body(const b2BodyDef* def)
{
	return m_pWorld->CreateBody(def);
}
//////////////////////////////////////////////////////////////////////////
void Box2DPhysicsSystem::destroy_body(b2Body* body)
{
	std::lock_guard<std::mutex> lock( m_debugDrawMutex);
	return m_pWorld->DestroyBody(body);
}
//////////////////////////////////////////////////////////////////////////
b2Joint* Box2DPhysicsSystem::create_joint(const b2JointDef* def)
{
	return m_pWorld->CreateJoint(def);
}
//////////////////////////////////////////////////////////////////////////
void Box2DPhysicsSystem::destroy_joint(b2Joint* joint)
{
	std::lock_guard<std::mutex> lock( m_debugDrawMutex);
	return m_pWorld->DestroyJoint(joint);
}
//////////////////////////////////////////////////////////////////////////
inline void Box2DPhysicsSystem::enable()	{ m_pContactListener->Enable(); }
//////////////////////////////////////////////////////////////////////////
inline void Box2DPhysicsSystem::disable()	{ m_pContactListener->Disable(); }
//////////////////////////////////////////////////////////////////////////
void Box2DPhysicsSystem::do_debug_draw()
{
	std::lock_guard<std::mutex> lock( m_debugDrawMutex);
	if( m_pWorld) m_pWorld->DrawDebugData();
}
//////////////////////////////////////////////////////////////////////////
void Box2DPhysicsSystem::set_drawer(const RendererInfo *renderInfo)
{	
#if PAKAL_USE_IRRLICHT
	m_pDebugDraw = new B2DebugDrawIrr(renderInfo->m_Device, renderInfo->m_Driver);
#endif
	m_pWorld->SetDebugDraw(m_pDebugDraw);
	m_pDebugDraw->SetFlags(b2Draw::e_shapeBit);
}

void Box2DPhysicsSystem::init_world()
{
	b2Vec2 gravity(0.00f, -9.82f);
	m_pWorld = new b2World(gravity);
	m_pWorld->SetWarmStarting(true);
	m_pWorld->SetContinuousPhysics(false);	
	m_pWorld->SetAllowSleeping(true);

	m_pContactListener = new ContactListener();
	m_pContactFilter = new ContactFilter();
	m_pDestructionListener = new DestructionListener();	

	m_pWorld->SetContactListener(m_pContactListener);
	m_pWorld->SetContactFilter(m_pContactFilter);
	m_pWorld->SetDestructionListener(m_pDestructionListener);

	m_pDebugDraw = nullptr;

	m_pWorld->SetDebugDraw(m_pDebugDraw);
}

void Box2DPhysicsSystem::clear_world()
{
	SAFE_DEL(m_pDebugDraw);
	SAFE_DEL(m_pContactListener);
	SAFE_DEL(m_pContactFilter);
	SAFE_DEL(m_pDestructionListener);
	SAFE_DEL(m_pWorld);
}


void Box2DPhysicsSystem::update_world(long long dt)
{
	float PHYSIC_UPDATE_RATE = dt/1000.0f;
	std::lock_guard<std::mutex> lock( m_debugDrawMutex) ;	
	m_pWorld->Step(PHYSIC_UPDATE_RATE, 8, 3);		
}


//////////////////////////////////////////////////////////////////////////