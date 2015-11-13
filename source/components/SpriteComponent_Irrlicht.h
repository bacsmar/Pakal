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




namespace Pakal
{
	class IrrGraphicsSystem;
	class SpriteAnimation;
	class SpriteNode_Irrlicht;

	class _PAKALExport SpriteComponent_Irrlicht : public SpriteComponent, public GraphicsSystem::IUpdatable
	{
		DECLARE_RTTI_WITH_BASE(SpriteComponent_Irrlicht, SpriteComponent);		

		unsigned			m_current_time;
		std::size_t			m_current_frame;
		bool				m_paused;
		IrrGraphicsSystem*	m_system;
		SpriteAnimation*	m_current_animation;

		SpriteNode_Irrlicht*	m_renderer;
		std::unordered_map<std::string, SpriteAnimation*>	m_animations;

		void load(std::istream& stream);

		void set_animation(SpriteAnimation& animation);
		void set_frame(size_t index, bool resetTime = true);

	public:	
		SpriteComponent_Irrlicht(IrrGraphicsSystem* system);
		~SpriteComponent_Irrlicht();		

		virtual BasicTaskPtr initialize(const Settings& settings) override;
		virtual BasicTaskPtr terminate() override;

		void set_animation(const std::string& name) override;

		void set_flipped(bool value) override;
		void set_looped(bool value) override;

		bool is_flipped() const override;
		bool is_looped() const override;

		void set_rotation(float degrees) override;
		void set_scale(const tmath::vector2df& factor) override;

		float get_rotation() const override;
		tmath::vector2df get_scale() const override;

		void play() override;
		void pause() override;
		void stop() override;
		bool is_playing() const override;

		void update(unsigned dt) override;
	};	

}

