#include "Config.h"
#include "Box2DPhysicsSystem.h"
#include "Box2DPhysicsListeners.h"

#include "ComponentFactory.h"
#include "components/BodyComponent.h"
#include "Components/BodyComponent_Box2D.h"
#include "Components/SpritePhysicsComponent_Box2D.h"
#include "LogMgr.h"

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
	LOG_INFO("[PhysicsSystem] Registering Box2D components");
	factories.emplace_back(CreateComponentFactory<BodyComponent,BodyComponent_Box2D>(this) );
	factories.emplace_back(CreateComponentFactory<SpritePhysicsComponent, SpritebodyComponent_Box2D>(this) );
}
//////////////////////////////////////////////////////////////////////////
b2Body* Box2DPhysicsSystem::create_body(const b2BodyDef* def)
{
	return m_world->CreateBody(def);
}
//////////////////////////////////////////////////////////////////////////
void Box2DPhysicsSystem::destroy_body(b2Body* body)
{
	mutex_guard lock( m_debug_draw_mutex);
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
	mutex_guard lock( m_debug_draw_mutex);
	return m_world->DestroyJoint(joint);
}
//////////////////////////////////////////////////////////////////////////
inline void Box2DPhysicsSystem::enable()	{ m_contact_listener->Enable(); }
//////////////////////////////////////////////////////////////////////////
inline void Box2DPhysicsSystem::disable()	{ m_contact_listener->Disable(); }
//////////////////////////////////////////////////////////////////////////
void Box2DPhysicsSystem::do_debug_draw()
{
	mutex_guard lock( m_debug_draw_mutex);
	if (m_world) 
	{
#if PAKAL_USE_IRRLICHT
		((B2DebugDrawIrr*)m_debug_draw)->update_camera();
#endif
		m_world->DrawDebugData();
	}
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
	LOG_INFO("[PhysicsSystem] Starting Box2D %d.%d.%d", b2_version.major, b2_version.minor, b2_version.revision);
	b2Vec2 gravity(m_settings.gravity.x, m_settings.gravity.y);
	m_world = new b2World(gravity);
	m_world->SetWarmStarting(m_settings.warm_starting);
	m_world->SetContinuousPhysics(m_settings.continuous_physics);
	m_world->SetAllowSleeping(m_settings.allow_sleep);

	m_contact_listener = new ContactListener();
	m_world->SetContactListener(m_contact_listener);
	//m_contact_filter = new ContactFilter();
	//m_destruction_listener = new DestructionListener();	

	//m_world->SetContactListener(m_contact_listener);
	//m_world->SetContactFilter(m_contact_filter);
	//m_world->SetDestructionListener(m_destruction_listener);	

	m_world->SetDebugDraw(m_debug_draw);

}

void Box2DPhysicsSystem::clear_world()
{
	mutex_guard lock(m_debug_draw_mutex);
	SAFE_DEL(m_debug_draw);
	SAFE_DEL(m_contact_listener);
	SAFE_DEL(m_contact_filter);
	SAFE_DEL(m_destruction_listener);
	SAFE_DEL(m_world);
}


void Box2DPhysicsSystem::update_world(unsigned long dt)
{
	static float targetTimeInSeconds = (1.f / 60); 
	static float targetTime = targetTimeInSeconds * 1000.f; // in milliseconds
	
	m_time_elapsed += dt;

	if (m_time_elapsed > targetTime)
	{
		mutex_guard lock(m_debug_draw_mutex);
		m_world->Step(m_time_elapsed*0.001f, m_settings.velocity_iterations, m_settings.position_iterations);
		m_time_elapsed = 0; 
	}

}
//////////////////////////////////////////////////////////////////////////