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
	class Sprite;
	class SpriteNode_Irrlicht;

	class _PAKALExport SpriteComponent_Irrlicht : public SpriteComponent, public GraphicsSystem::IUpdatable
	{
		DECLARE_RTTI_WITH_BASE(SpriteComponent_Irrlicht, SpriteComponent);		

		unsigned			m_current_time;
		std::size_t			m_current_frame;
		bool				m_paused;
		IrrGraphicsSystem*	m_system;
		Sprite*	m_current_animation;
		SpriteSheetPtr		m_sprite_sheet;

		SpriteNode_Irrlicht*	m_node;
		std::unordered_map<std::string, Sprite*>	m_animations;
		unsigned m_current_frame_time;
		void load(const SpriteSheet& spriteSheet);

		void set_animation(Sprite& animation);
		void set_frame(size_t index, bool resetTime = true);		

		float m_size_factor;
		float m_normalization_factor;
		void normalize_size(const tmath::vector2du & size);

	public:	
		SpriteComponent_Irrlicht(IrrGraphicsSystem* system);
		~SpriteComponent_Irrlicht();		

		virtual BasicTaskPtr initialize(const Settings& settings) override;
		virtual BasicTaskPtr terminate() override;

		void set_visible(bool visible) override;
		bool is_visible() override;

		void set_animation(const std::string& name) override;
		void set_animation(unsigned index) override;
		unsigned get_animation_count() const override;

		void set_flipped(bool value) override;
		void set_looped(bool value) override;

		bool is_flipped() const override;
		bool is_looped() const override;

		void set_rotation(float degrees) override;
		void set_scale(const tmath::vector3df& factor) override;
		void set_size(float size) override;

		void set_position(const tmath::vector3df & position) override;
		tmath::vector3df get_position() const override;

		virtual unsigned get_duration() const override;

		float get_rotation() const override;
		tmath::vector3df get_scale() const override;
		float get_normalization_factor() const override;

		void set_height(float height, bool keep_relation);

		void play() override;
		void pause() override;
		void stop() override;
		bool is_playing() const override;

		void update(unsigned dt) override;
	};	

}

