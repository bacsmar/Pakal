#include "BodyComponent_Box2D.h"
#include "box2D/Box2DPhysicsSystem.h"

using namespace Pakal;

BodyComponent_Box2D::~BodyComponent_Box2D()
{
	m_system = nullptr;
	ASSERT(m_body == nullptr);
}

BasicTaskPtr BodyComponent_Box2D::initialize(const Settings& settings)
{
	return m_system->execute_block([=]()
	{
		b2BodyDef bodyDef;
		bodyDef.position.Set( settings.position.x, settings.position.y);

		switch (settings.type)
		{
			case BodyType::Static: bodyDef.type = b2_staticBody; break;
			case BodyType::Kinematic: bodyDef.type = b2_kinematicBody; break;
			case BodyType::Dynamic: bodyDef.type = b2_dynamicBody; break;
			default: break;
		}

	
		b2FixtureDef fixtureDef;
		fixtureDef.density = settings.density;
		fixtureDef.friction = settings.friction;
		fixtureDef.restitution = settings.restitution;

		std::shared_ptr<b2Shape> shape;
		if (settings.shape_info->getType() == CircleShape::getRTTI())
		{
			CircleShape* s =  static_cast<CircleShape*>(settings.shape_info);

			shape = std::make_shared<b2CircleShape>();
			shape->m_radius = s->radius;

			fixtureDef.shape = shape.get();
		}
		else
		{
			//TODO implement other shapes
			ASSERT(false);
		}

		m_body = m_system->create_body(&bodyDef);
		m_fixture =  m_body->CreateFixture(&fixtureDef);	
	});	
}

BasicTaskPtr BodyComponent_Box2D::destroy()
{
	return m_system->execute_block([=]()
	{
		m_system->destroy_body(m_body);
		m_body = nullptr;		
	});
}

void BodyComponent_Box2D::set_density(float density)
{
	m_fixture->SetDensity(density);
}

void BodyComponent_Box2D::set_friction(float friction)
{
	m_fixture->SetFriction(friction);
}

void BodyComponent_Box2D::set_restitution(float restitution)
{
	m_fixture->SetRestitution(restitution);
}

float BodyComponent_Box2D::get_density()
{
	return m_fixture->GetDensity();
}

float BodyComponent_Box2D::get_friction()
{
	return m_fixture->GetFriction();
}

float BodyComponent_Box2D::get_restitution()
{
	return m_fixture->GetRestitution();
}

tmath::vector3df BodyComponent_Box2D::get_position()
{
	auto& v = m_body->GetPosition();
	return tmath::vector3df(v.x,v.y,0.0f);
}

BasicTaskPtr BodyComponent_Box2D::set_position(const tmath::vector3df & position)
{
	return m_system->execute_block([=]()
	{
		m_body->SetTransform(b2Vec2(position.x,position.y),0);
	});
}