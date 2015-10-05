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
class SpriteIrrlicht;

namespace Pakal
{
	
	class Entity;
	class IrrGraphicsSystem;
	class IStream;
	
	class _PAKALExport SpriteComponent_Irrlicht : public SpriteComponent, public GraphicsSystem::IUpdatable
	{
		friend class Entity;
	public:
		DECLARE_RTTI_WITH_BASE(SpriteComponent_Irrlicht, SpriteComponent);		
			
		SpriteComponent_Irrlicht(IrrGraphicsSystem *irrManager);
		virtual ~SpriteComponent_Irrlicht(void);		

		virtual BasicTaskPtr initialize(const Settings& settings) override;
		virtual BasicTaskPtr finalize() override;

		void set_animation(const std::string& animationName) override;
		void set_animation(const SpriteIrrlicht& animation);
		void setLooped(bool looped) override;

		void update(unsigned dt) override;

		inline bool get_flipped() const override { return m_isFlipped; } 
		inline void set_flipped(bool val) override { m_isFlipped = val; }

	protected:	
		bool load(IStream * stream);

		bool isLooped() const;
		bool isPlaying() const;

		void setFrame(std::size_t frameIndex, bool resetTime = true);
		void setFrameTime(unsigned time);
		unsigned getFrameTime() const;

		void play();
		void play(const SpriteIrrlicht& animation);
		void pause();
		void stop();		
		const SpriteIrrlicht* getAnimation() const;

		const SpriteIrrlicht* m_sprite;

		unsigned			m_frameTime;
		unsigned			m_currentTime;

		std::size_t			m_currentFrame;
		bool				m_isPaused;
		bool				m_isLooped;
		bool				m_isFlipped;

		irr::IrrlichtDevice * m_device;		
		irr::video::IVideoDriver* m_driver;
		IrrGraphicsSystem*			m_system;
				
		SpriteNode_Irrlicht *m_sprite_node;
		std::unordered_map<std::string, SpriteIrrlicht*>	m_sprites;
	};	

}

