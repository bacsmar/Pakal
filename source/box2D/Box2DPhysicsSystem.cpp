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
Box2DPhysicsSystem::Box2DPhysicsSystem(const Settings& settings) :
		PhysicsSystem(settings),
		m_world(nullptr),
		m_contact_listener(nullptr),
		m_contact_filter(nullptr),
		m_destruction_listener(nullptr),
		m_debug_draw(nullptr),
		m_time_elapsed(0)
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
	return m_world->CreateBody(def);
}
//////////////////////////////////////////////////////////////////////////
void Box2DPhysicsSystem::destroy_body(b2Body* body)
{
	std::lock_guard<std::mutex> lock( m_debug_draw_mutex);
	return m_world->DestroyBody(body);
}
//////////////////////////////////////////////////////////////////////////
b2Joint* Box2DPhysicsSystem::create_joint(const b2JointDef* def)
{
	return m_world->CreateJoint(def);
}
//////////////////////////////////////////////////////////////////////////
void Box2DPhysicsSystem::destroy_joint(b2Joint* joint)
{
	std::lock_guard<std::mutex> lock( m_debug_draw_mutex);
	return m_world->DestroyJoint(joint);
}
//////////////////////////////////////////////////////////////////////////
inline void Box2DPhysicsSystem::enable()	{ m_contact_listener->Enable(); }
//////////////////////////////////////////////////////////////////////////
inline void Box2DPhysicsSystem::disable()	{ m_contact_listener->Disable(); }
//////////////////////////////////////////////////////////////////////////
void Box2DPhysicsSystem::do_debug_draw()
{
	std::lock_guard<std::mutex> lock( m_debug_draw_mutex);
	if( m_world) m_world->DrawDebugData();
}
//////////////////////////////////////////////////////////////////////////
void Box2DPhysicsSystem::set_drawer(const RendererInfo *renderInfo)
{	
#if PAKAL_USE_IRRLICHT
	m_debug_draw = new B2DebugDrawIrr(renderInfo->m_Device, renderInfo->m_Driver);
#endif
	m_world->SetDebugDraw(m_debug_draw);
	m_debug_draw->SetFlags(b2Draw::e_shapeBit);
}

void Box2DPhysicsSystem::init_world()
{
	
	b2Vec2 gravity(m_settings.gravity.x, m_settings.gravity.y);
	m_world = new b2World(gravity);
	m_world->SetWarmStarting(true);
	m_world->SetContinuousPhysics(false);	
	m_world->SetAllowSleeping(m_settings.allow_sleep);

	//m_contact_listener = new ContactListener();
	//m_contact_filter = new ContactFilter();
	//m_destruction_listener = new DestructionListener();	

	//m_world->SetContactListener(m_contact_listener);
	//m_world->SetContactFilter(m_contact_filter);
	//m_world->SetDestructionListener(m_destruction_listener);

	m_debug_draw = nullptr;

	m_world->SetDebugDraw(m_debug_draw);

}

void Box2DPhysicsSystem::clear_world()
{
	SAFE_DEL(m_debug_draw);
	SAFE_DEL(m_contact_listener);
	SAFE_DEL(m_contact_filter);
	SAFE_DEL(m_destruction_listener);
	SAFE_DEL(m_world);
}


void Box2DPhysicsSystem::update_world(long long dt)
{
	std::lock_guard<std::mutex> lock(m_debug_draw_mutex);

	/*static float targetTimeInSeconds = (1.f / 60); 
	static long long targetTime = targetTimeInSeconds * 1000; // in milliseconds
	
	m_time_elapsed += dt;	// in milliseconds
	while(m_time_elapsed > targetTime)
	{ 
		m_world->Step( targetTimeInSeconds , m_settings.velocity_iterations, m_settings.position_iterations);
		m_time_elapsed -= targetTime;
	}
	*/
	m_world->Step( dt * 0.001f, m_settings.velocity_iterations, m_settings.position_iterations);
}
//////////////////////////////////////////////////////////////////////////