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
	: m_frameTime(200), m_currentTime(0), m_currentFrame(0), m_isPaused(true),
	m_isLooped(true), m_isFlipped(false), m_system(irrManager), m_sprite_node(nullptr)
{
	m_driver = irrManager->get_driver();
	m_device = irrManager->get_device();
}


SpriteComponent_Irrlicht::~SpriteComponent_Irrlicht( void )
{			
}

bool SpriteComponent_Irrlicht::load( IStream* stream)
{
	pugi::xml_document xmlFile;
	
	std::vector<char> buffer(static_cast<unsigned>(stream->size()));	
	stream->read( &buffer[0], stream->size());

	pugi::xml_parse_result result = xmlFile.load_buffer(buffer.data(), buffer.size());

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

	std::string defaultAnim = animationsNode.attribute("default_anim").as_string();

	LOG_DEBUG("[SpriteComponent] texture: %s, defaultAnim: %s", textureName, defaultAnim.c_str());

	auto texture = m_driver->getTexture(textureName);
	if(texture == nullptr)
	{
		LOG_ERROR("[SpriteComponent] texture: %s not found!", textureName);
		return false;
	}

	//<animation name="name" frame_time="5">
	for (const auto& framesNode : animationsNode)
	{
		std::string animationName = framesNode.attribute("name").as_string();
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
		
		//animation->set_sprite_sheet(*texture);
		
		for (auto&& frame : framesNode)
		{			
			//<frame left="0" top="0" width="64" height="64"/>
			int left	= frame.attribute(SPRITE_FRAME_LEFT).as_int(-1);
			int top		= frame.attribute(SPRITE_FRAME_TOP).as_int(-1);
			int width	= frame.attribute(SPRITE_FRAME_WIDTH).as_int(-1);
			int height	= frame.attribute(SPRITE_FRAME_HEIGHT).as_int(-1);
			int offsetX	= frame.attribute("oX").as_int(0);
			int offsetY	= frame.attribute("oY").as_int(0);
			animation->add_frame(tmath::recti(left, top, width, height), tmath::vector2di(offsetX, offsetY) );			
		}				
	}

	m_sprite_node->set_texture(texture);

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
		auto node = m_device->getSceneManager()->getRootSceneNode();
		m_sprite_node = new SpriteNode_Irrlicht(node, m_device->getSceneManager(), -1);

		m_frameTime = settings.frame_time_ms;
		m_isPaused = settings.init_paused;
		m_isLooped = settings.init_looped;

		auto resource = ResourceManager::instance().open_resource(settings.resource_file, false);
		IStream* resourceStream = resource.get();

		if( resourceStream)
			load( resourceStream );
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
		if (m_currentTime >= m_frameTime)
		{
			// reset time, but keep the remainder
			//m_currentTime = sf::microseconds(m_currentTime.asMicroseconds() % m_frameTime.asMicroseconds());
			m_currentTime = ((int)(m_currentTime * 1000) % (int)(m_frameTime));

			// get next Frame index
			if (m_currentFrame + 1 < m_sprite->get_size())
			{
				++m_currentFrame;
			}
			else
			{
				// animation has ended
				m_currentFrame = 0; // reset to start

				if (!m_isLooped)
				{
					m_isPaused = true;
				}
			}

			// set the current frame, not reseting the time
			set_frame(m_currentFrame, false);
		}
	}
}

bool SpriteComponent_Irrlicht::is_looped() const
{
	return m_isLooped;
}

bool SpriteComponent_Irrlicht::is_playing() const
{
	return !m_isPaused;
}

void SpriteComponent_Irrlicht::set_frame(size_t frameIndex, bool resetTime)
{
	ASSERT(m_sprite_node);
	m_sprite_node->set_frame(frameIndex, m_sprite);
	if (resetTime)
		m_currentTime = 0;
}

unsigned SpriteComponent_Irrlicht::get_frame_time() const
{
	return m_frameTime;
}

void SpriteComponent_Irrlicht::set_animation(const Sprite& animation)
{
	m_sprite = &animation;
	//m_sprite_node->set_texture(m_sprite->get_sprite_sheet());
	m_currentFrame = 0;
	set_frame(m_currentFrame);
}

void SpriteComponent_Irrlicht::set_animation(const std::string& animationName)
{
	const auto& defaultAnimation = m_sprites.find(animationName);
	if (defaultAnimation != m_sprites.end())
	{
		set_animation(*defaultAnimation->second);
	}
	else
	{
		LOG_ERROR("[SpriteComponent] missing animation: %s", animationName.c_str());
	}
}

void SpriteComponent_Irrlicht::set_frame_fime(unsigned time)
{
	m_frameTime = time;
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
	set_frame(m_currentFrame);
}

void SpriteComponent_Irrlicht::set_looped(bool looped)
{
	m_isLooped = looped;
}

const Sprite* SpriteComponent_Irrlicht::get_animation() const
{
	return m_sprite;
}