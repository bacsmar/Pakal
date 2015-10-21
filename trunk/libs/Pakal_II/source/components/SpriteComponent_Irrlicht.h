///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2011 The MetalEngine Open Source Project
// File: SpriteComponent_Irrlicht.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: 01-11-2012
//
// Purpose: 
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "Platform.h"
#include "SpriteComponent.h"
#include "RTTI.h"
#include <unordered_map>

#include "Irrlicht.h"
#include "GraphicsSystem.h"



class SpriteNode_Irrlicht;

namespace Pakal
{
	
	class Entity;
	class IrrGraphicsSystem;
	class Sprite;
	
	class _PAKALExport SpriteComponent_Irrlicht : public SpriteComponent, public GraphicsSystem::IUpdatable
	{
		DECLARE_RTTI_WITH_BASE(SpriteComponent_Irrlicht, SpriteComponent);		
		friend class Entity;
	public:
			
		SpriteComponent_Irrlicht(IrrGraphicsSystem *irrManager);
		virtual ~SpriteComponent_Irrlicht(void);		

		virtual BasicTaskPtr initialize(const Settings& settings) override;
		virtual BasicTaskPtr terminate() override;

		void set_animation(const std::string& animationName) override;				
		bool is_looped() const override;
		void play() override;
		void pause() override;
		void stop() override;
		bool is_playing() const override;

		void update(unsigned dt) override;

		inline bool get_flipped() const override { return m_is_flipped; } 
		inline void set_flipped(bool val) override { m_is_flipped = val; }

	protected:	
		bool load(std::istream* stream);		

		void set_animation(const Sprite& animation);
		void play(const Sprite& animation);
		void set_sprite_node_frame(std::size_t frameIndex, bool resetTime = true);
		
		unsigned	get_frame_time() const;
		
		const Sprite* get_animation() const;
		
		unsigned			m_currentTime;
		std::size_t			m_currentFrame;
		bool				m_isPaused;		
		bool				m_is_flipped;
		const Sprite*		m_sprite = nullptr;
		IrrGraphicsSystem*	m_system;
				
		SpriteNode_Irrlicht*		m_sprite_node;
		std::unordered_map<std::string, Sprite*>	m_sprites;
	};	

}

