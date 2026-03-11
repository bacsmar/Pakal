#include "SpritePhysicsComponent_Box2D.h"
#include "box2D/Box2DPhysicsSystem.h"
#include "math/tmg.h"

#include "components/SpritePhysics.h"

#include "LogMgr.h"
#include <algorithm>
#include <cmath>


using namespace Pakal;

SpritebodyComponent_Box2D::~SpritebodyComponent_Box2D()
{
	m_system = nullptr;
}

BasicTaskPtr SpritebodyComponent_Box2D::initialize(const Settings& _loader)
{	
	m_scale = _loader.scale;
	return m_system->execute_block([this, _loader]()	// copy the smartpointer, just to keep our data alive.
	{
		auto& loader = _loader.sprite_physics;		// we are only interested (for now) in the sprite_physics
		if (!loader || loader->bodies.empty())
		{
			ASSERT_MSG(loader != nullptr, "[SpritebodyComponent] SpriteSheetPhysics loader is null");
			return;
		}
		// bodies
		for(auto spriteBody : loader->bodies)
		{
			b2BodyDef bodydef;
			bodydef.type = spriteBody->dynamic ? b2_dynamicBody : b2_staticBody;
			bodydef.awake = spriteBody->awake;
			bodydef.gravityScale = spriteBody->gravity_scale;
			bodydef.allowSleep = spriteBody->allow_sleep;

			auto body = m_system->create_body(&bodydef);

			//body->SetTransform( b2Vec2(animation-> position.x, animation->position.y), body->GetAngle());
			body->SetFixedRotation(spriteBody->fixed_rotation);			
			body->GetUserData().pointer = reinterpret_cast<uintptr_t>(this);

			body->SetTransform(b2Vec2(_loader.position.x, _loader.position.y), body->GetAngle());
			if (_loader.fixed_rotation.has_value())
			{
				body->SetFixedRotation(_loader.fixed_rotation.value());
			}
			if (_loader.body_type.has_value())
			{
				body->SetType(_loader.body_type.value() == DynamicBody ? b2_dynamicBody : b2_staticBody);
			}
			if (_loader.gravity_scale.has_value())
			{
				body->SetGravityScale(_loader.gravity_scale.value());
			}
			if (_loader.lineal_velocity.has_value())
			{
				body->SetLinearVelocity({ _loader.lineal_velocity->x, _loader.lineal_velocity->y });
			}

			m_bodies[spriteBody->name] = body;

			// fixtures
			unsigned fixtureIndex = 0;
			for(const auto& fixture : spriteBody->m_fixtures)
			{
				b2FixtureDef fixtureDef;				

				fixtureDef.density = fixture.density;
				fixtureDef.friction = fixture.friction;
				fixtureDef.restitution = fixture.restitution;
				fixtureDef.isSensor = fixture.is_sensor;
				fixtureDef.filter.maskBits = fixture.mask_bits;
				fixtureDef.filter.categoryBits = fixture.category_bits;
				//fixtureDef.filter
				//fixtureDef.userData


				if (fixture.type == "CIRCLE")
				{
					b2CircleShape shape;
					shape.m_radius = fixture.m_circle.r * m_scale * fixture.scale;
					shape.m_p = { fixture.m_circle.x * m_scale * fixture.scale, fixture.m_circle.y * m_scale * fixture.scale };

					fixtureDef.shape = &shape;
					auto fixturePtr = body->CreateFixture(&fixtureDef);
					fixturePtr->GetUserData().pointer = static_cast<uintptr_t>(fixtureIndex++);
					m_fixtures.emplace_back(fixturePtr);
				}
				else // "POLYGON"
				{					
					//polygon
					for(const auto& polygon : fixture.m_polygons)
					{
						std::vector<b2Vec2> vertices;
						for( const auto& vertex : polygon.m_vertices)
						{
							vertices.emplace_back(b2Vec2(vertex.x * m_scale * fixture.scale, vertex.y * m_scale * fixture.scale));
						}
						b2PolygonShape shape;
						shape.Set(&vertices[0], vertices.size());
						fixtureDef.shape = &shape;
						auto fixturePtr = body->CreateFixture(&fixtureDef);
						fixturePtr->GetUserData().pointer = static_cast<uintptr_t>(fixtureIndex++);
						m_fixtures.emplace_back(fixturePtr);
					}
				}								
			}
		}
		if (!m_bodies.empty())
		{
			m_active_body = m_bodies.begin()->second;
			if (_loader.initial_impulse.has_value())
			{
				m_active_body->ApplyLinearImpulse({ _loader.initial_impulse->x, _loader.initial_impulse->y }, m_active_body->GetWorldCenter(), true);
			}
			if (_loader.initial_force.has_value())
			{
				m_active_body->ApplyForceToCenter({ _loader.initial_force->x, _loader.initial_force->y }, true);
			}
		}
		else
		{
			LOG_ERROR("[SpritebodyComponent] no active body created after initialization");
		}
	});
}

BasicTaskPtr SpritebodyComponent_Box2D::terminate()
{	
	return m_system->execute_block([this]()
	{
		m_active_body = nullptr;
		m_fixtures.clear();
		for( auto& body : m_bodies)
		{
			m_system->destroy_body(body.second);
		}
		m_bodies.clear();
	});
}

tmath::vector3df SpritebodyComponent_Box2D::get_position()
{
	ASSERT_MSG(m_active_body, "[body not yet initialized]");
	auto& v = m_active_body->GetPosition();
	return tmath::vector3df(v.x,v.y,0.0f);
}

BasicTaskPtr SpritebodyComponent_Box2D::set_position(const tmath::vector3df & position)
{
	ASSERT_MSG(m_active_body, "[body not yet initialized]");
	return m_system->execute_block([this, position]()
	{
		m_active_body->SetTransform(b2Vec2(position.x,position.y),m_active_body->GetAngle());
	});
}

BasicTaskPtr SpritebodyComponent_Box2D::set_angle(const tmath::vector3df& angle)
{
	ASSERT_MSG(m_active_body, "[body not yet initialized]");
	return m_system->execute_block([this, angle]()
	{
		m_active_body->SetTransform(m_active_body->GetPosition(),tmg::d2r(angle.x));
	});
}

tmath::vector3df SpritebodyComponent_Box2D::get_angle()
{
	ASSERT_MSG(m_active_body, "[body not yet initialized]");
	return tmath::vector3df(0,0,tmg::r2d(m_active_body->GetAngle()));	
}

void SpritebodyComponent_Box2D::set_scale(float scale)
{
	m_scale = scale;
}

float SpritebodyComponent_Box2D::get_scale()
{
	return m_scale;
}

void SpritebodyComponent_Box2D::apply_impulse(const tmath::vector2df& force)
{
	m_system->execute_block([this, force]()
	{
		ASSERT_MSG(m_active_body, "[body not yet initialized]");
		m_active_body->ApplyLinearImpulse({ force.x, force.y }, m_active_body->GetWorldCenter(), true);
	});
}

void SpritebodyComponent_Box2D::apply_force(const tmath::vector2df& force)
{
	m_system->execute_block([this, force]()
	{
		ASSERT_MSG(m_active_body, "[body not yet initialized]");
		m_active_body->ApplyForceToCenter({ force.x, force.y }, true);
	});
}

tmath::vector2df SpritebodyComponent_Box2D::get_lineal_velocity() const
{
	ASSERT_MSG(m_active_body, "[body not yet initialized]");
	auto velocity = m_active_body->GetLinearVelocity();
	return {velocity.x, velocity.y};
}

void SpritebodyComponent_Box2D::set_lineal_velocity(const tmath::vector2df& velocity)
{
	m_system->execute_block([this, velocity]()
	{
		ASSERT_MSG(m_active_body, "[body not yet initialized]");
		m_active_body->SetLinearVelocity({ velocity.x,velocity.y });
	});
}

bool SpritebodyComponent_Box2D::fixed_rotation() const
{
	ASSERT_MSG(m_active_body, "[body not yet initialized]");
	return m_active_body->IsFixedRotation();
}

// must be used after initialization, otherwise it will have no effect until the next initialization
void SpritebodyComponent_Box2D::set_fixed_rotation(bool val)
{
	m_system->execute_block([this, val]()
	{
		ASSERT_MSG(m_active_body, "[body not yet initialized]");
		m_active_body->SetFixedRotation(val);
	});
}

void SpritebodyComponent_Box2D::set_type(BodyType type)
{	
	//auto b2Type = type == DynamicBody ? b2_dynamicBody : (type == KinematicBody ? b2_kinematicBody : b2_staticBody);
	auto b2Type = type == DynamicBody ? b2_dynamicBody : b2_staticBody;

	m_system->execute_block([this, b2Type]()
	{
		ASSERT_MSG(m_active_body, "[body not yet initialized]");
		m_active_body->SetType(b2Type);
	});	
}

void SpritebodyComponent_Box2D::set_gravity_scale(float gravityScale)
{
	m_system->execute_block([this, gravityScale]()
	{
		ASSERT_MSG(m_active_body, "[body not yet initialized]");
		m_active_body->SetGravityScale(gravityScale);
	});
}

SpritePhysicsComponent::BodyPart SpritebodyComponent_Box2D::get_bodyPart(unsigned index)
{
	ASSERT(m_fixtures.size() > index);
	auto fixture = m_fixtures[index];
	return { fixture->GetDensity(), fixture->GetFriction(), fixture->GetRestitution(), fixture->IsSensor(), static_cast<unsigned>(fixture->GetUserData().pointer) };
}

void SpritebodyComponent_Box2D::update_bodyPart(const BodyPart& part)
{
	ASSERT(part.id < m_fixtures.size());

	auto fixture = m_fixtures[part.id];
	fixture->SetDensity(part.density);
	fixture->SetFriction(part.friction);
	fixture->SetRestitution(part.restitution);
	fixture->SetSensor(part.is_sensor);
}
