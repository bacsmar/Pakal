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

#include "irrlicht/Sprite_Irrlicht.hpp"
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

Pakal::SpriteComponent_Irrlicht::SpriteComponent_Irrlicht(Pakal::IrrGraphicsSystem *irrManager)
	: m_Flipped(false), m_Texture(nullptr), m_system(irrManager), m_sprite_node(nullptr)
{
	m_driver = irrManager->get_driver();
	m_device = irrManager->get_device();
}


Pakal::SpriteComponent_Irrlicht::~SpriteComponent_Irrlicht( void )
{		
	SAFE_DEL(m_sprite_node);
	for (const auto& it : m_sprites)
	{
		delete it.second;
	}
	m_sprites.clear();
}

bool Pakal::SpriteComponent_Irrlicht::load( IStream* stream)
{
	pugi::xml_document xmlFile;
	
	std::vector<char> buffer(stream->size());	
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

	if( (m_Texture = m_driver->getTexture(textureName)) == nullptr)
	{
		LOG_ERROR("[SpriteComponent] texture: %s not found!", textureName);
		return false;
	}

	//<animation name="name" frame_time="5">
	for (const auto& framesNode : animationsNode)
	{
		std::string animationName = framesNode.attribute("name").as_string();
		SpriteIrrlicht *animation = nullptr;

		const auto& it =  m_sprites.find(animationName);		
		if( it != m_sprites.end() )
		{
			LOG_WARNING("[SpriteComponent]  animation %s already declared", animationName);
			animation = it->second;
		}else
		{
			animation = new SpriteIrrlicht();
			m_sprites[animationName] = animation;
		}
		
		animation->setSpriteSheet(*m_Texture);
		
		for (auto&& frame : framesNode)
		{			
			//<frame left="0" top="0" width="64" height="64"/>
			int left	= frame.attribute(SPRITE_FRAME_LEFT).as_int(-1);
			int top		= frame.attribute(SPRITE_FRAME_TOP).as_int(-1);
			int width	= frame.attribute(SPRITE_FRAME_WIDTH).as_int(-1);
			int height	= frame.attribute(SPRITE_FRAME_HEIGHT).as_int(-1);
			int offsetX	= frame.attribute("oX").as_int(0);
			int offsetY	= frame.attribute("oY").as_int(0);
			animation->addFrame(core::recti(left, top, width, height), core::vector2di(offsetX, offsetY) );			
		}				
	}		

	const auto& defaultAnimation =  m_sprites.find(defaultAnim);	
	if( defaultAnimation != m_sprites.end())
	{
		m_sprite_node->setAnimation( *defaultAnimation->second);
	}
	else
	{
		m_sprite_node->setAnimation( *m_sprites.begin()->second );
	}	

	return true;	
}

Pakal::BasicTaskPtr Pakal::SpriteComponent_Irrlicht::initialize(const Settings& settings)
{
	return m_system->execute_block([=]() 
	{

		ASSERT(m_sprite_node == nullptr);
		auto node = m_device->getSceneManager()->getRootSceneNode();
		m_sprite_node = new SpriteNode_Irrlicht(node, m_device->getSceneManager(), -1, settings.frame_time_ms, settings.init_paused, settings.init_looped);

		auto resource = ResourceManager::instance().open_resource(settings.resource_file, false);
		load( resource.get() );

		m_sprite_node->setPosition(core::vector3df(settings.initial_position.x, settings.initial_position.y, settings.initial_position.z));
		m_sprite_node->setAnimation(settings.initial_animation);
		m_system->add_to_update_list(this);
	});
}

Pakal::BasicTaskPtr Pakal::SpriteComponent_Irrlicht::destroy()
{
	return m_system->execute_block([=]() 
	{
		m_system->remove_from_update_list(this);
		ASSERT(m_sprite_node);		
		m_sprite_node->drop();
	});
}

void Pakal::SpriteComponent_Irrlicht::update(unsigned dt)
{
	if (m_sprite_node)
	{
		m_sprite_node->update(dt);
	}
}