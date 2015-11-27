#include "SpritebodyComponent_Box2D.h"
#include "box2D/Box2DPhysicsSystem.h"
#include "math/tmg.h"

#include "Components/SpritePhysics.h"
#include "IEntity.h"

#include "logMgr.h"


using namespace Pakal;

SpritebodyComponent_Box2D::~SpritebodyComponent_Box2D()
{
	m_system = nullptr;
}

BasicTaskPtr SpritebodyComponent_Box2D::initialize(const SpritePhysicsLoader& loader)
{
	return m_system->execute_block([=]()
	{
		// bodies
		for(auto animation :loader.animations)
		{
			b2BodyDef bodydef;
			bodydef.type = animation->dynamic ? b2_dynamicBody : b2_staticBody;
			bodydef.awake = true;
			auto body = m_system->create_body(&bodydef);

			m_bodies[animation->name] = body;
			// fixtures
			for(const auto& fixture : animation->m_fixtures)
			{
				b2FixtureDef fixtureDef;
				std::unique_ptr<b2Shape> pshape;

				if (fixture.type == "CIRCLE")
				{
					auto shape = new b2CircleShape;
					pshape.reset(shape);
					shape->m_radius = fixture.m_circle.r *sqrt(m_scale.x*m_scale.x+ m_scale.y*m_scale.y);
					shape->m_p = { fixture.m_circle.x * m_scale.x, fixture.m_circle.y * m_scale.y};
				}
				else // "POLYGON"
				{
					auto shape = new b2PolygonShape;
					pshape.reset(shape);
					//polygn
					for(const auto& polygon : fixture.m_polygons)
					{
						std::vector<b2Vec2> vertices;
						for( const auto& vertex : polygon.m_vertices)
						{
							//vertices.emplace_back( b2Vec2(vertex.x, vertex.y) );
							vertices.emplace_back( b2Vec2(vertex.x * m_scale.x, vertex.y * m_scale.y) );
						}
						shape->Set(&vertices[0], vertices.size());
					}
				}
				fixtureDef.density = fixture.density;
				fixtureDef.friction = fixture.friction;
				fixtureDef.restitution = fixture.restitution;
				fixtureDef.isSensor = fixture.is_sensor;
				fixtureDef.shape = pshape.get();
				//fixtureDef.filter 
				//fixtureDef.userData
				body->CreateFixture(&fixtureDef);
			}
		}
		m_active_body = m_bodies.begin()->second;
		m_active_body->SetFixedRotation(m_fixed_rotation);
		m_active_body->SetUserData(this);		
		auto mass = m_active_body->GetMass();
	});
}

BasicTaskPtr SpritebodyComponent_Box2D::terminate()
{
	return m_system->execute_block([=]()
	{
		for( auto& body : m_bodies)
		{
			m_system->destroy_body(body.second);
		}
		m_bodies.clear();
	});
}

tmath::vector3df SpritebodyComponent_Box2D::get_position()
{
	auto& v = m_active_body->GetPosition();
	return tmath::vector3df(v.x,v.y,0.0f);
}

BasicTaskPtr SpritebodyComponent_Box2D::set_position(const tmath::vector3df & position)
{
	return m_system->execute_block([=]()
	{
		m_active_body->SetTransform(b2Vec2(position.x,position.y),m_active_body->GetAngle());
	});
}

BasicTaskPtr SpritebodyComponent_Box2D::set_angle(const tmath::vector3df& angle)
{
	return m_system->execute_block([=]()
	{
		m_active_body->SetTransform(m_active_body->GetPosition(),tmg::d2r(angle.x));
	});
}

tmath::vector3df SpritebodyComponent_Box2D::get_angle()
{
	return tmath::vector3df(0,0,tmg::r2d(m_active_body->GetAngle()));	
}

void SpritebodyComponent_Box2D::set_scale(const tmath::vector3df& scale)
{
	m_scale = scale;
}

tmath::vector3df SpritebodyComponent_Box2D::get_scale()
{
	return m_scale;
}

void SpritebodyComponent_Box2D::apply_impulse(const tmath::vector2df& force)
{
	m_active_body->ApplyLinearImpulse({force.x, force.y}, m_active_body->GetWorldCenter(), true);
}

void SpritebodyComponent_Box2D::apply_force(const tmath::vector2df& force)
{
	m_active_body->ApplyForceToCenter({ force.x, force.y }, true);
}

tmath::vector2df SpritebodyComponent_Box2D::get_lineal_velocity() const
{
	auto velocity = m_active_body->GetLinearVelocity();
	return {velocity.x, velocity.y};
}

void SpritebodyComponent_Box2D::set_lineal_velocity(const tmath::vector2df& velocity)
{
	m_active_body->SetLinearVelocity({ velocity.x,velocity.y });
}

bool SpritebodyComponent_Box2D::on_collide(const PhysicComponent& other)
{
	auto otherEntity = other.get_parent_entity();
	auto thisEntity = this->get_parent_entity();	
	return true;
}