#include "BodyComponent_Box2D.h"

#if PAKAL_USE_BOX2D == 1

#include "box2D/Box2DPhysicsSystem.h"

using namespace Pakal;

void BodyComponent_Box2D::on_initialize()
{	
	//std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	b2BodyDef bodyDef;
	bodyDef.type = b2_dynamicBody;
	bodyDef.position.Set( 3.0f, 2.1f);

	b2CircleShape shape;
	shape.m_radius = 20.22f;

	b2FixtureDef fixtureDef;
	fixtureDef.shape = &shape;
	fixtureDef.density = 2.37f;
	fixtureDef.friction = 0.31f;
	fixtureDef.restitution = 0.82f;

	m_body = get_system()->createBody(&bodyDef);
	m_fixture =  m_body->CreateFixture(&fixtureDef);	
	
}

void BodyComponent_Box2D::on_destroy()
{
	get_system()->destroyBody(m_body);
	m_body = nullptr;
}

BodyComponent_Box2D::~BodyComponent_Box2D()
{
}

BodyComponent_Box2D::BodyComponent_Box2D(Box2DPhysicsSystem* box) : BodyComponent(box), m_body(nullptr)
{
}

b2Body* BodyComponent_Box2D::createBody(b2BodyDef& bodyDef)
{
	ASSERT( !m_body);
	m_body = get_system()->createBody(&bodyDef);
	return m_body;
}

b2Fixture* BodyComponent_Box2D::addFixture(b2FixtureDef& fixtureDef)
{
	ASSERT(m_body);

	m_fixture =  m_body->CreateFixture(&fixtureDef);
	return m_fixture;
}

tmath::vector3df & BodyComponent_Box2D::getPosition()
{
	ASSERT(m_body);
	auto b2Vec2 = m_body->GetPosition();
	m_position.x = b2Vec2.x;	
	m_position.y = b2Vec2.y;	
	return m_position;
}

void BodyComponent_Box2D::setPosition(const tmath::vector3df & position)
{
	m_position = position;
}

Box2DPhysicsSystem* BodyComponent_Box2D::get_system()
{
	return static_cast<Box2DPhysicsSystem*>(m_system);
}

#endif