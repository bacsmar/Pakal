#pragma once
#include "Component.h"
#include "BasicTask.h"
#include "PakalMath.h"
#include <vector>

namespace Pakal
{
	class _PAKALExport TileMapComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(TileMapComponent, Component);
	public:
		struct Settings
		{
			std::string tilemap_file;
			std::string sprite_sheet_file;
		};		

		//  C C C B B C C C
		//  C C C A A C C C
		//
		// B = ladrillo, A = interrogacion, C = vacio
		// world_tilemap
		// area_tilemap
		// spritesheet

		virtual BasicTaskPtr initialize(const Settings& settings) = 0;
		virtual BasicTaskPtr terminate() = 0;

		using WorldTileMap = std::vector<std::vector<int>>;


		virtual void set_tile_map(const WorldTileMap& tileMap) = 0;
		virtual void draw_area(const tmath::recti& m_tile_map_area) = 0;
		
		virtual ~TileMapComponent(){}
		

	protected:
		//tmath::recti m_tile_map_area;
		//WorldTileMap m_world_tilemap;

	};
}
