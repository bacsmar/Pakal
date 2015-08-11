#include "BodyComponent.h"
#include "box2D/Box2DPhysicsSystem.h"

#include <functional>
#include <iostream>

void Pakal::BodyComponent::onInit()
{	
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

	m_body = getSystem()->createBody(&bodyDef);
	m_fixture =  m_body->CreateFixture(&fixtureDef);	

	std::function<void(bool)> f = [=](bool)
	{
		std::cout << "Joder:"<< this->m_body->GetPosition().y<<std::endl;
	};
	
	listenerId = getSystem()->updatEvent.addListener(f);	
}

void Pakal::BodyComponent::onDestroy()
{
	getSystem()->updatEvent.removeListener(listenerId);
	getSystem()->destroyBody(m_body);
	m_body = 0;
	std::cout << "eliminando:"<<std::endl;
}

Pakal::BodyComponent::~BodyComponent()
{
}

Pakal::BodyComponent::BodyComponent(Box2DPhysicsSystem* box) : PhysicComponent(box), m_body(nullptr)
{
}

b2Body* Pakal::BodyComponent::createBody(b2BodyDef& bodyDef)
{
	ASSERT( !m_body);
	m_body = getSystem()->createBody(&bodyDef);
	return m_body;
}

b2Fixture* Pakal::BodyComponent::addFixture(b2FixtureDef& fixtureDef)
{
	ASSERT(m_body);

	m_fixture =  m_body->CreateFixture(&fixtureDef);
	return m_fixture;
}

Pakal::core::vector3df & Pakal::BodyComponent::getPosition()
{
	ASSERT(m_body);
	auto b2Vec2 = m_body->GetPosition();
	m_Position.X = b2Vec2.x;	
	m_Position.Y = b2Vec2.y;	
	return m_Position;
}

void Pakal::BodyComponent::setPosition(Pakal::core::vector3df & position)
{
	m_Position = position;
}

Pakal::Box2DPhysicsSystem* Pakal::BodyComponent::getSystem()
{
	return static_cast<Box2DPhysicsSystem*>( getPhysicsSystem() );
}