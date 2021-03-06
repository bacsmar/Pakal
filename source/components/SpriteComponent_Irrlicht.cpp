#include "SpriteComponent_Irrlicht.h"

#include "Components/Sprite.h"
#include "irrlicht/SpriteNode_Irrlicht.hpp"
#include "irrlicht/IrrGraphicsSystem.h"

#include "LogMgr.h"
#include "persist/XmlReader.h"
#include "ResourceManager.h"
#include <irrlicht/SpriteBatcher.hpp>


using namespace Pakal;
using namespace irr;
using namespace core;

SpriteComponent_Irrlicht::SpriteComponent_Irrlicht(IrrGraphicsSystem* system)
	: m_current_time(0), m_current_frame(0), m_paused(true), m_system(system), m_current_animation(nullptr), m_node(nullptr), m_current_frame_time(0), m_size_factor(1.f), m_normalization_factor(1.f)
{ }

SpriteComponent_Irrlicht::~SpriteComponent_Irrlicht() 
{ }

void SpriteComponent_Irrlicht::set_flipped(bool val)
{
	vector3df currentRotation = m_node->getRotation();
	currentRotation.Y = val ? 180.f : 0.f;
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

void SpriteComponent_Irrlicht::set_scale(const tmath::vector3df& factor)
{
	m_node->setScale(vector3df(factor.x, factor.y, 1));
}

void SpriteComponent_Irrlicht::set_size(float size)
{
	m_size_factor = size;
	m_node->setScale(irr::core::vector3df(1.f,1.f,1.f) * m_normalization_factor * m_size_factor);
}

void SpriteComponent_Irrlicht::set_position(const tmath::vector3df & position)
{
	m_node->setPosition(core::vector3df(position.x,position.y,position.z));
}

tmath::vector3df SpriteComponent_Irrlicht::get_position() const
{
	const auto& posVector = m_node->getPosition();
	return tmath::vector3df(posVector.X,posVector.Y, posVector.Z);
}

unsigned SpriteComponent_Irrlicht::get_duration() const
{
	return m_current_animation->duration;
}

float SpriteComponent_Irrlicht::get_rotation() const
{
	return m_node->getRotation().Z;
}

tmath::vector3df SpriteComponent_Irrlicht::get_scale() const
{
	const auto & currentScale = m_node->getScale();
	return { currentScale.X, currentScale.Y ,1.f};
}

float SpriteComponent_Irrlicht::get_normalization_factor() const
{
	return m_normalization_factor;
}

void SpriteComponent_Irrlicht::set_height(float height, bool keep_relation)
{
	// TODO
	//f64 length = ref_size.x*ref_size.x + ref_size.y*ref_size.y;
	//auto normalization_factor = (f32)core::reciprocal_squareroot(length);

	//auto sen = std::sin(ref_size.y * normalization_factor);

	//// height / sen
}

BasicTaskPtr SpriteComponent_Irrlicht::initialize(const Settings& settings)
{
	return m_system->execute_block([=]()
	{
		ASSERT(m_node == nullptr);

		if (settings.is_batched == false)
		{
			m_node = new SpriteNode_Irrlicht(m_system->get_device()->getSceneManager()->getRootSceneNode(),
				m_system->get_device()->getSceneManager(),
				m_system->get_material_manager());
		}else
		{
			m_node = new SpriteNode_Irrlicht(nullptr,
				m_system->get_device()->getSceneManager(),
				m_system->get_material_manager());

			m_system->get_sprite_batcher()->add_sprite(m_node);
		}

		m_paused = settings.init_paused;
		m_size_factor = settings.size;
		m_node->setPosition(vector3df(settings.position.x, settings.position.y, settings.position.z));
		
		m_sprite_sheet = settings.sprite_sheet;
		load(*settings.sprite_sheet.get());
		if(m_animations.size() > 0)
			m_system->add_to_update_list(this);

	});
}

BasicTaskPtr SpriteComponent_Irrlicht::terminate()
{
	ASSERT(m_node);

	return m_system->execute_block([=]() 
	{
		if (m_animations.size() > 0)
			m_system->remove_from_update_list(this);
		m_system->get_sprite_batcher()->remove_sprite(m_node);
		m_node->detach();
		m_node->drop();		
		m_animations.clear();
	});
}

void SpriteComponent_Irrlicht::set_visible(bool visible)
{
	m_node->setVisible(visible);
}

bool SpriteComponent_Irrlicht::is_visible()
{
	return m_node->isVisible();
}

void SpriteComponent_Irrlicht::load(const SpriteSheet& spriteSheet)
{	
	auto texture = m_system->get_driver()->getTexture(spriteSheet.texture_name.c_str());
	if (texture == nullptr)
	{
		LOG_ERROR("[SpriteComponent] texture: %s not found!", spriteSheet.texture_name.c_str());
		return;
	}
	m_node->set_texture(texture);

	for(auto animation : spriteSheet.animations)
	{
		m_animations[animation->name] = animation;
	}	

	normalize_size({ spriteSheet.pixels_scale, spriteSheet.pixels_scale});
	set_size(spriteSheet.meters_scale);

	const auto& defaultAnimation = m_animations.find(spriteSheet.default_animation);
	if (defaultAnimation != m_animations.end())
	{
		set_animation(*defaultAnimation->second);
	}
	else
	{
		auto animation = *m_animations.begin()->second;		
		LOG_WARNING("[SpriteComponent] no default animation set, using first animation found %s", animation.name.c_str() );
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
		if (m_current_time >= m_current_frame_time)
		{
			// reset time, but keep the remainder			
			m_current_time = m_current_time % m_current_frame_time;

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

void SpriteComponent_Irrlicht::normalize_size(const tmath::vector2du & size)
{
	f64 length = size.x*size.x + size.y*size.y;
	if (length == 0)
	{
		LOG_WARNING("[SpriteComponent] missing reference size in SpriteSheet");
		return;
	}
	m_normalization_factor = (f32)core::reciprocal_squareroot(length);
}

void SpriteComponent_Irrlicht::set_animation(Sprite& animation)
{
	m_current_animation = &animation;
	m_current_frame = 0;
	m_paused = false;
	ASSERT(m_current_animation->get_size() > 0);	// the animation should have at least 1 frame
	m_current_frame_time = static_cast<unsigned>(m_current_animation->duration / (m_current_animation->get_size() *1.f));
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

void SpriteComponent_Irrlicht::set_animation(unsigned index)
{
	ASSERT(m_animations.size() >= index);

	if (m_animations.size() >= index)
	{
		unsigned i = 0;
		for (auto &animation : m_animations)
		{
			if (i == index)
			{
				set_animation(*animation.second);
				return;
			}
			i++;
		}
	}
}

unsigned SpriteComponent_Irrlicht::get_animation_count() const
{
	return m_animations.size();
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