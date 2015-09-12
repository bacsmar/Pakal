#include "BodyComponent_Box2D.h"
#include "box2D/Box2DPhysicsSystem.h"
#include "math/tmg.h"

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

		switch (settings.shape_info->m_shape_type)
		{
			case ShapeType::CircleShape: 
			{
				CircleShape* c = static_cast<CircleShape*>(settings.shape_info);
				auto cshape = std::make_shared<b2CircleShape>();
				cshape->m_radius = c->radius;
				shape = cshape;
			} 
			break;
			case ShapeType::PolygonShape: 
			{
				PolygonShape* p = static_cast<PolygonShape*>(settings.shape_info);

				b2Vec2* vertices = new b2Vec2[p->vertices.size()]; 

				for (size_t i = 0; i < p->vertices.size(); i++)
					vertices[i] = b2Vec2(p->vertices[i].x,p->vertices[i].y);

				auto pshape = std::make_shared<b2PolygonShape>();

				pshape->Set(vertices,p->vertices.size());

				delete vertices;

				shape = pshape;
			} 
			break;
			case ShapeType::BoxShape:
			{
				BoxShape* b = static_cast<BoxShape*>(settings.shape_info);

				auto pshape = std::make_shared<b2PolygonShape>();

				float hx = b->dimensions.x / 2.0f;
				float hy = b->dimensions.y / 2.0f;
				float angle =  tmg::d2r(b->angle.x);
				b2Vec2 centroid = b2Vec2(b->center.x,b->center.y);

				pshape->SetAsBox(hx,hy,centroid,angle);

				shape = pshape;
			}
			break;
			case ShapeType::EdgeShape:
			case ShapeType::ChainShape:
			default:
				ASSERT(false);
			break;

		}
		fixtureDef.shape = shape.get();

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
		m_body->SetTransform(b2Vec2(position.x,position.y),m_body->GetAngle());
	});
}

BasicTaskPtr BodyComponent_Box2D::set_angle(const tmath::vector3df& angle)
{
	return m_system->execute_block([=]()
	{
		m_body->SetTransform(m_body->GetPosition(),tmg::d2r(angle.x));
	});
}

tmath::vector3df BodyComponent_Box2D::get_angle()
{
	return tmath::vector3df(0,0,tmg::r2d(m_body->GetAngle()));
}

void BodyComponent_Box2D::set_size(const tmath::vector3df& size)
{
	ASSERT(false); // implement me :(
}

tmath::vector3df BodyComponent_Box2D::get_size()
{
	ASSERT(false); // implement me :(
	return tmath::vector3df();
}