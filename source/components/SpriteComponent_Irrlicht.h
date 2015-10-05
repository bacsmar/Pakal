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

		bool load(IStream * stream);		
		void setFlipped(bool val) { m_Flipped = val; }

		virtual BasicTaskPtr initialize(const Settings& settings) override;
		virtual BasicTaskPtr destroy() override;

		void update(unsigned dt) override;

	protected:	

		bool m_Flipped;

		irr::IrrlichtDevice * m_device;
		irr::video::ITexture* m_Texture;
		irr::video::IVideoDriver* m_driver;
		IrrGraphicsSystem*			m_system;
				
		SpriteNode_Irrlicht *m_sprite_node;
		std::unordered_map<std::string, SpriteIrrlicht*>	m_sprites;

		bool Flipped() const { return m_Flipped; }
	};	

}

