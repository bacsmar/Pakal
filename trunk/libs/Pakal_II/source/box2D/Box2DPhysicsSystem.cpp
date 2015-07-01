#include "Config.h"
#include "Box2DPhysicsSystem.h"
#include "Box2DPhysicsListeners.h"
#include "IComponent.h"

#include "InboxQueue.h"
#include "Task.h"

// TODO: Encontrar una mejor forma de hacer esta mierda.. un componente?
#if PAKAL_USE_IRRLICHT
	#include "box2d/B2DebugDrawIrr.h"	
#endif

using namespace Pakal;

//////////////////////////////////////////////////////////////////////////
Box2DPhysicsSystem::~Box2DPhysicsSystem()
{	
}
//////////////////////////////////////////////////////////////////////////
Box2DPhysicsSystem::Box2DPhysicsSystem() :
		m_pWorld(0),
		m_pContactListener(0),
		m_pContactFilter(0),
		m_pDestructionListener(0),
		m_pDebugDraw(0)		
{	
}
//////////////////////////////////////////////////////////////////////////
void Box2DPhysicsSystem::update()
{				
	float PHYSIC_UPDATE_RATE = 1.0f/30.0f;
	std::lock_guard<std::mutex> lock( m_debugDrawMutex) ;	
	m_pWorld->Step(PHYSIC_UPDATE_RATE, 8, 3);		

}
//////////////////////////////////////////////////////////////////////////
void Box2DPhysicsSystem::initWorld()
{	
	b2Vec2 gravity(0.0f, 0.0f);
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
//////////////////////////////////////////////////////////////////////////
void Box2DPhysicsSystem::clearWorld()
{
	SAFE_DEL(m_pDebugDraw);
	SAFE_DEL(m_pContactListener);
	SAFE_DEL(m_pContactFilter);
	SAFE_DEL(m_pDestructionListener);
	SAFE_DEL(m_pWorld);
}
//////////////////////////////////////////////////////////////////////////
void Box2DPhysicsSystem::registerComponentFactories( std::vector<IComponentFactory*> &factories )
{

}
//////////////////////////////////////////////////////////////////////////
BasicTask * Box2DPhysicsSystem::initComponentAsync(IComponent *c) 
{
	std::function<int()> lambdaInit = [&] (void) { c->init(); return 0; };

	return getInbox()->pushTask( lambdaInit ).get();
}
//////////////////////////////////////////////////////////////////////////
BasicTask * Box2DPhysicsSystem::terminateComponentAsync(IComponent *c) 
{
	return nullptr;
}
//////////////////////////////////////////////////////////////////////////
b2Body* Box2DPhysicsSystem::createBody(const b2BodyDef* def)
{
	return m_pWorld->CreateBody(def);
}
//////////////////////////////////////////////////////////////////////////
void Box2DPhysicsSystem::destroyBody(b2Body* body)
{
	return m_pWorld->DestroyBody(body);
}
//////////////////////////////////////////////////////////////////////////
b2Joint* Box2DPhysicsSystem::createJoint(const b2JointDef* def)
{
	return m_pWorld->CreateJoint(def);
}
//////////////////////////////////////////////////////////////////////////
void Box2DPhysicsSystem::destroyJoint(b2Joint* joint)
{
	return m_pWorld->DestroyJoint(joint);
}
//////////////////////////////////////////////////////////////////////////
inline void Box2DPhysicsSystem::enable()	{ m_pContactListener->Enable(); }
//////////////////////////////////////////////////////////////////////////
inline void Box2DPhysicsSystem::disable()	{ m_pContactListener->Disable(); }
//////////////////////////////////////////////////////////////////////////
void Box2DPhysicsSystem::doDebugDraw()
{
	std::lock_guard<std::mutex> lock( m_debugDrawMutex);
	if( m_pWorld) m_pWorld->DrawDebugData();
}
//////////////////////////////////////////////////////////////////////////
void Box2DPhysicsSystem::setDrawer(const RendererInfo *renderInfo)
{	
#if PAKAL_USE_IRRLICHT
	m_pDebugDraw = new B2DebugDrawIrr(renderInfo->m_Device, renderInfo->m_Driver);
#endif
}
//////////////////////////////////////////////////////////////////////////