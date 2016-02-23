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
#include "RTTI.h"

#include "Irrlicht.h"
#include "GraphicsSystem.h"
#include "TileMapComponent.h"
#include "irrlicht/TiledNode_Irrlicht.hpp"


namespace Pakal
{
	class IrrGraphicsSystem;
	class Sprite;
	class SpriteNode_Irrlicht;

	class _PAKALExport TileMapComponent_Irrlicht : public TileMapComponent, public GraphicsSystem::IUpdatable
	{		
	private:
		DECLARE_RTTI_WITH_BASE(TileMapComponent_Irrlicht, TileMapComponent);
		

	public:
		explicit TileMapComponent_Irrlicht(IrrGraphicsSystem* system);
		~TileMapComponent_Irrlicht();

		virtual BasicTaskPtr initialize(const Settings& settings) override;
		virtual BasicTaskPtr terminate() override;

		void update(unsigned dt) override;
		void set_tile_map(const WorldTileMap& tileMap) override;
		void draw_area(const tmath::recti& m_tile_map_area) override;

	protected:

		void load_tilemap(std::istream& stream);
		void load_sprite_sheet(std::istream& stream, std::string& outTextureName);
		void set_size(float size);
		void normalize_size(tmath::vector2du size);
		WorldTileMap m_world_tilemap;
		IrrGraphicsSystem* m_system;

		TiledNode_Irrlicht* m_node;
		std::unordered_map<int, Sprite*>	m_animations;

		tmath::vector2di draw_area_size = {400,200};
		irr::f32 m_normalization_factor;
		float m_size_factor;
	};	

}

