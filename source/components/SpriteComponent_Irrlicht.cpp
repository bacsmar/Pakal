///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The PakalEngine Open Source Project
// File: ASpriteComponent.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: 01-11-2012
//
// Purpose: 
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#include "SpriteComponent_Irrlicht.h"

#include "Components/Sprite.h"
#include "irrlicht/SpriteNode_Irrlicht.hpp"
#include "irrlicht/IrrGraphicsSystem.h"

#include "LogMgr.h"
#include "pugixml.hpp"
#include "ResourceManager.h"

const char * SPRITE_FILE_HEADER = "SpriteSheetAnimation";
const char * SPRITE_FILE_TEXTURE = "texture";
const char * SPRITE_FILE_DEFAULT_ANIM = "default";

const char * SPRITE_FRAME_LEFT = "x";
const char * SPRITE_FRAME_TOP = "y";
const char * SPRITE_FRAME_WIDTH = "w";
const char * SPRITE_FRAME_HEIGHT = "h";


using namespace Pakal;
using namespace irr;

SpriteComponent_Irrlicht::SpriteComponent_Irrlicht(IrrGraphicsSystem *irrManager)
	: m_currentTime(0), m_currentFrame(0), m_isPaused(true),
	m_is_flipped(false), m_system(irrManager), m_sprite_node(nullptr)
{
}


SpriteComponent_Irrlicht::~SpriteComponent_Irrlicht( void )
{			
}

bool SpriteComponent_Irrlicht::load(std::istream* stream)
{
	pugi::xml_document xmlFile;
	
	pugi::xml_parse_result result = xmlFile.load(*stream);

	if( result.status != pugi::status_ok )
	{
		LOG_DEBUG("[SpriteComponent] Invalid stream file");
		return false;
	}

	// search for the signature
	pugi::xml_node animationsNode = xmlFile.child(SPRITE_FILE_HEADER);

	if( animationsNode == nullptr)
	{
		LOG_DEBUG("[SpriteComponent] Invalid signature");
		return false;
	}

	const char* textureName = animationsNode.attribute("texture").as_string();

	std::string defaultAnim = animationsNode.attribute( SPRITE_FILE_DEFAULT_ANIM ).as_string();

	LOG_DEBUG("[SpriteComponent] texture: %s, defaultAnim: %s", textureName, defaultAnim.c_str());

	auto texture = m_system->get_driver()->getTexture(textureName);
	if(texture == nullptr)
	{
		LOG_ERROR("[SpriteComponent] texture: %s not found!", textureName);
		return false;
	}

	//<animation name="name" frame_time="5">
	for (const auto& animationNode : animationsNode)
	{
		std::string animationName = animationNode.attribute("name").as_string();		
		Sprite *animation = nullptr;

		const auto& it =  m_sprites.find(animationName);		
		if( it != m_sprites.end() )
		{
			LOG_WARNING("[SpriteComponent]  animation %s already declared", animationName.c_str());
			animation = it->second;
		}else
		{
			animation = new Sprite();
			m_sprites[animationName] = animation;
		}
		
		animation->duration = animationNode.attribute("duration").as_uint(100);
		animation->is_looped = animationNode.attribute("looped").as_bool();		
		
		for (const auto& frame : animationNode)
		{			
			//<frame left="0" top="0" width="64" height="64"/>
			int left	= frame.attribute(SPRITE_FRAME_LEFT).as_int(-1);
			int top		= frame.attribute(SPRITE_FRAME_TOP).as_int(-1);
			int width	= frame.attribute(SPRITE_FRAME_WIDTH).as_int(-1);
			int height	= frame.attribute(SPRITE_FRAME_HEIGHT).as_int(-1);

			float offsetX	= frame.attribute("oX").as_int() - frame.attribute("pX").as_float();
			float offsetY	= frame.attribute("oY").as_int() - frame.attribute("pY").as_float();

			animation->add_frame(tmath::recti(left, top, width, height), tmath::vector2df(offsetX, offsetY) );			
		}		
		
	}

	m_sprite_node->set_texture(texture);

	ASSERT(m_sprites.empty() == false);

	const auto& defaultAnimation =  m_sprites.find(defaultAnim);	
	if( defaultAnimation != m_sprites.end())
	{
		set_animation( *defaultAnimation->second);
	}
	else
	{
		set_animation( *m_sprites.begin()->second );
	}	

	return true;	
}

BasicTaskPtr SpriteComponent_Irrlicht::initialize(const Settings& settings)
{
	return m_system->execute_block([=]() 
	{
		ASSERT(m_sprite_node == nullptr);
		auto node = m_system->get_device()->getSceneManager()->getRootSceneNode();
		m_sprite_node = new SpriteNode_Irrlicht(node, m_system->get_device()->getSceneManager(), -1);
		
		m_isPaused = settings.init_paused;

		auto resource = ResourceManager::instance().open_read_resource(settings.resource_file, false);

		if (resource)
		{
			load(resource.get());
		}
		else
		{
			LOG_ERROR("[SpriteComponent]  invalid resource %s", settings.resource_file.c_str());
		}

		m_sprite_node->setPosition(core::vector3df(settings.initial_position.x, settings.initial_position.y, settings.initial_position.z));
		set_animation(settings.initial_animation);
		m_system->add_to_update_list(this);
	});
}

BasicTaskPtr SpriteComponent_Irrlicht::terminate()
{
	return m_system->execute_block([=]() 
	{
		m_system->remove_from_update_list(this);
		ASSERT(m_sprite_node);
		m_sprite_node->detach();
		m_sprite_node->drop();
		for (const auto& it : m_sprites)
		{
			delete it.second;
		}
		m_sprites.clear();

	});
}

void SpriteComponent_Irrlicht::update(unsigned deltaTime)
{
	// if not paused and we have a valid animation
	if (!m_isPaused && m_sprite)
	{
		// add delta time
		m_currentTime += deltaTime;

		// if current time is bigger then the frame time advance one frame		
		if (m_currentTime >= m_sprite->duration)
		{
			// reset time, but keep the remainder			
			m_currentTime = (m_currentTime ) % (m_sprite->duration);

			// get next Frame index
			if (m_currentFrame + 1 < m_sprite->get_size())
			{
				++m_currentFrame;
			}
			else
			{
				if (!m_sprite->is_looped)
				{
					m_isPaused = true;
					fire_event_animation_ended();
				}
				else
				{
					// animation has ended
					m_currentFrame = 0; // reset to start 
				}
			}

			// set the current frame, not reseting the time			
			set_sprite_node_frame(m_currentFrame, false);			
		}
	}
}

bool SpriteComponent_Irrlicht::is_looped() const
{
	ASSERT(m_sprite != nullptr);
	return m_sprite->is_looped;
}

bool SpriteComponent_Irrlicht::is_playing() const
{
	return !m_isPaused;
}

void SpriteComponent_Irrlicht::set_sprite_node_frame(size_t frameIndex, bool resetTime)
{
	ASSERT(m_sprite_node != nullptr);
	ASSERT(m_sprite != nullptr);
	m_system->execute_block([=]()
	{
		m_sprite_node->set_frame(frameIndex, *m_sprite, m_is_flipped);
		if (resetTime)
			m_currentTime = 0;
	});	
}

unsigned SpriteComponent_Irrlicht::get_frame_time() const
{
	ASSERT(m_sprite != nullptr);
	return m_sprite->duration;
}

void SpriteComponent_Irrlicht::set_animation(const Sprite& animation)
{
	m_sprite = &animation;
	m_currentFrame = 0;
	m_isPaused = false;
	set_sprite_node_frame(m_currentFrame);
}

void SpriteComponent_Irrlicht::set_animation(const std::string& animationName)
{
	const auto& animation = m_sprites.find(animationName);
	if (animation != m_sprites.end())
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
	m_isPaused = false;
}

void SpriteComponent_Irrlicht::play(const Sprite& animation)
{
	if (get_animation() != &animation)
		set_animation(animation);
	play();
}

void SpriteComponent_Irrlicht::pause()
{
	m_isPaused = true;
}

void SpriteComponent_Irrlicht::stop()
{
	m_isPaused = true;
	m_currentFrame = 0;
	set_sprite_node_frame(m_currentFrame);
}

const Sprite* SpriteComponent_Irrlicht::get_animation() const
{
	return m_sprite;
}