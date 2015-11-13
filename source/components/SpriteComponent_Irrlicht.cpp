#include "SpriteComponent_Irrlicht.h"

#include "Components/Sprite.h"
#include "irrlicht/SpriteNode_Irrlicht.hpp"
#include "irrlicht/IrrGraphicsSystem.h"

#include "LogMgr.h"
#include "XmlReader.h"
#include "ResourceManager.h"


using namespace Pakal;
using namespace irr;
using namespace core;

SpriteComponent_Irrlicht::SpriteComponent_Irrlicht(IrrGraphicsSystem* system)
	: m_current_time(0), m_current_frame(0), m_paused(true), m_system(system), m_node(nullptr)
{ }

SpriteComponent_Irrlicht::~SpriteComponent_Irrlicht() 
{ }

void SpriteComponent_Irrlicht::set_flipped(bool val)
{
	vector3df currentRotation = m_node->getRotation();
	currentRotation.Y = val ? 0.f : 180.f;
	m_node->setRotation(currentRotation);
}

void SpriteComponent_Irrlicht::set_looped(bool value)
{
	m_current_animation->looped = value;
}

bool SpriteComponent_Irrlicht::is_flipped() const
{
	return m_node->getRotation().Y == 0.f;
}

void SpriteComponent_Irrlicht::set_rotation(float degrees)
{
	vector3df currentRotation = m_node->getRotation();
	currentRotation.Z = degrees;
	m_node->setRotation(currentRotation);
}

void SpriteComponent_Irrlicht::set_scale(const tmath::vector2df& factor)
{		
	m_node->setScale(vector3df(factor.x, factor.y, 1));
}

float SpriteComponent_Irrlicht::get_rotation() const
{
	return m_node->getRotation().Z;
}

tmath::vector2df SpriteComponent_Irrlicht::get_scale() const
{
	auto currentScale = m_node->getScale();
	return { currentScale.X, currentScale.Y };
}


BasicTaskPtr SpriteComponent_Irrlicht::initialize(const Settings& settings)
{
	return m_system->execute_block([=]() 
	{
		ASSERT(m_node == nullptr);

		m_node = 
			new SpriteNode_Irrlicht(m_system->get_device()->getSceneManager()->getRootSceneNode(), m_system->get_device()->getSceneManager());

		m_paused = settings.init_paused;

		if (auto resource = rmgr.open_read_resource(settings.resource_file, false))
		{
			load(*resource);
		}
		else
		{
			LOG_ERROR("[SpriteComponent]  invalid resource %s", settings.resource_file.c_str());
		}

		m_node->setPosition(vector3df(settings.position.x, settings.position.y, settings.position.z));
		m_system->add_to_update_list(this);

	});
}

BasicTaskPtr SpriteComponent_Irrlicht::terminate()
{
	ASSERT(m_node);

	return m_system->execute_block([=]() 
	{
		m_system->remove_from_update_list(this);
		
		m_node->detach();
		m_node->drop();
		for (const auto& it : m_animations)
		{
			delete it.second;
		}
		m_animations.clear();
	});
}

void SpriteComponent_Irrlicht::load(std::istream& stream)
{
	SpriteLoader loader;

	XmlReader reader;
	reader.read(stream, "SpriteSheetAnimation", loader);


	auto texture = m_system->get_driver()->getTexture(loader.texture_name.c_str());
	if (texture == nullptr)
	{
		LOG_ERROR("[SpriteComponent] texture: %s not found!", loader.texture_name.c_str());
		return;
	}
	m_node->set_texture(texture);

	for(SpriteAnimation* animation : loader.animations)
	{
		m_animations[animation->name] = animation;
	}

	const auto& defaultAnimation = m_animations.find(loader.default_animation);
	if (defaultAnimation != m_animations.end())
	{
		set_animation(*defaultAnimation->second);
	}
	else
	{
		set_animation(*m_animations.begin()->second);
	}
}


void SpriteComponent_Irrlicht::update(unsigned deltaTime)
{
	// if not paused and we have a valid animation
	if (!m_paused && m_current_animation)
	{
		// add delta time
		m_current_time += deltaTime;

		// if current time is bigger then the frame time advance one frame		
		if (m_current_time >= m_current_animation->duration)
		{
			// reset time, but keep the remainder			
			m_current_time = m_current_time % m_current_animation->duration;

			// get next Frame index
			if (m_current_frame + 1 < m_current_animation->get_size())
			{
				++m_current_frame;
			}
			else
			{
				if (!m_current_animation->looped)
				{
					m_paused = true;
					event_animation_ended.notify();
				}
				else
				{
					// animation has ended
					m_current_frame = 0; // reset to start 
				}
			}

			// set the current frame, not reseting the time			
			set_frame(m_current_frame, false);			
		}
	}
}

bool SpriteComponent_Irrlicht::is_looped() const
{
	return m_current_animation->looped;
}

bool SpriteComponent_Irrlicht::is_playing() const
{
	return !m_paused;
}

void SpriteComponent_Irrlicht::set_frame(size_t frameIndex, bool resetTime)
{
	ASSERT(m_node != nullptr && m_current_animation != nullptr);
	
	m_system->execute_block([=]()
	{
		m_node->set_frame(frameIndex, *m_current_animation);

		if (resetTime)
		{
			m_current_time = 0;
		}
	});	
}

void SpriteComponent_Irrlicht::set_animation(SpriteAnimation& animation)
{
	m_current_animation = &animation;
	m_current_frame = 0;
	m_paused = false;
	set_frame(m_current_frame);
}

void SpriteComponent_Irrlicht::set_animation(const std::string& animationName)
{
	const auto& animation = m_animations.find(animationName);
	if (animation != m_animations.end())
	{
		set_animation(*animation->second);
	}
	else
	{
		LOG_ERROR("[SpriteComponent] missing animation: %s", animationName.c_str());
	}
}

void SpriteComponent_Irrlicht::play()
{
	m_paused = false;
}

void SpriteComponent_Irrlicht::pause()
{
	m_paused = true;
}

void SpriteComponent_Irrlicht::stop()
{
	m_paused = true;
	m_current_frame = 0;
	set_frame(m_current_frame);
}