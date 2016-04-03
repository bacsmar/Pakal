#include "TileMapComponent_Irrlicht.h"

#include "Components/Sprite.h"
#include "irrlicht/TiledNode_Irrlicht.hpp"
#include "irrlicht/IrrGraphicsSystem.h"

#include "LogMgr.h"
#include "persist/XmlReader.h"
#include "ResourceManager.h"


using namespace Pakal;
using namespace irr;
using namespace core;

// NOTE: THIS COMPONENT IS NOT READY YET

TileMapComponent_Irrlicht::TileMapComponent_Irrlicht(IrrGraphicsSystem* system) : m_system(system), m_node(nullptr)
{
	LOG_WARNING("TileMapComponent_Irrlicht THIS COMPONENT IS NOT READY YET");
}

TileMapComponent_Irrlicht::~TileMapComponent_Irrlicht()
{ }

BasicTaskPtr TileMapComponent_Irrlicht::initialize(const Settings& settings)
{
	return m_system->execute_block([=]()
	{
		ASSERT(m_node == nullptr);		
				
		auto tilemap_file = ResourceMgr.open_read_resource(settings.tilemap_file, false);
		auto sprite_sheet_file = ResourceMgr.open_read_resource(settings.sprite_sheet_file, false);		

		if (tilemap_file && sprite_sheet_file)
		{
			load_tilemap(*tilemap_file);
			std::string textureName;
			load_sprite_sheet(*sprite_sheet_file, textureName);

			auto texture = m_system->get_driver()->getTexture(textureName.c_str());
			if (texture == nullptr)
			{
				LOG_ERROR("[TileMapComponent] texture: %s not found!", textureName.c_str());
				return;
			}

			m_node = new TiledNode_Irrlicht(m_system->get_device()->getSceneManager()->getRootSceneNode(), m_system->get_device()->getSceneManager(), m_system->get_material_manager());
			m_node->set_texture(texture);

			set_size(3.0);
			//m_node->create_tilemap_drawer({50,40}, {4,2} );			
			draw_area({2,1,4,2});
			//m_node->setPosition(vector3df(settings.position.x, settings.position.y, settings.position.z));
		}		
		else
		{
			LOG_ERROR("[TileMapComponent]  invalid resources %s %s", settings.tilemap_file.c_str(), settings.sprite_sheet_file.c_str());
			return;
		}		

		m_system->add_to_update_list(this);

	});
}

BasicTaskPtr TileMapComponent_Irrlicht::terminate()
{
	return m_system->execute_block([=]()
	{		

	});
}

void TileMapComponent_Irrlicht::update(unsigned dt)
{
}

void TileMapComponent_Irrlicht::set_tile_map(const WorldTileMap& tileMap)
{
}

void TileMapComponent_Irrlicht::draw_area(const tmath::recti& m_tile_map_area)
{
	ASSERT(m_node != nullptr );

	m_system->execute_block([=]()
	{
		float offset_x = (float)draw_area_size.x/m_tile_map_area.size.x;
		float offset_y = (float)draw_area_size.y/m_tile_map_area.size.y;

		for (int row = 0; row < m_tile_map_area.size.x; ++row)
		{
			for (int col = 0; col < m_tile_map_area.size.y; ++col)
			{				
				auto spriteIndex = m_world_tilemap[col + m_tile_map_area.left_corner.y][row + m_tile_map_area.left_corner.x];
				m_node->set_tile(0, *m_animations[spriteIndex], { offset_x * col , offset_y * row } );
			}
		}		
	});
}

void TileMapComponent_Irrlicht::load_tilemap(std::istream& stream)
{
	m_world_tilemap = 
	{ 
		{0, 0, 0, 0, 0, 0, 0, 0 },
		{0, 0, 0, 2, 2, 0, 0, 0 },
		{0, 0, 0, 1, 1, 0, 0, 0 },
		{0, 0, 0, 0, 0, 0, 0, 0 },
	};
}

void TileMapComponent_Irrlicht::load_sprite_sheet(std::istream& stream, std::string& outTextureName)
{
	SpriteSheet loader;

	XmlReader reader;
	reader.read(stream, "SpriteSheetAnimation", loader);	
	
	outTextureName = loader.texture_name;	

	int index = 0;
	for (auto animation : loader.animations)
	{		
		m_animations[index] = animation;
		index++;
	}

	normalize_size({ loader.pixels_scale, loader.pixels_scale });
}

void TileMapComponent_Irrlicht::normalize_size(tmath::vector2du size)
{
	//TODO
	f64 length = size.x*size.x + size.y*size.y;
	if (length == 0)
	{
		LOG_WARNING("[SpriteComponent] missing reference size in SpriteSheet");
		return;
	}
	m_normalization_factor = (f32)core::reciprocal_squareroot(length);
}

void TileMapComponent_Irrlicht::set_size(float size)
{
	m_size_factor = size;
	m_node->setScale(irr::core::vector3df(1.f, 1.f, 1.f) * m_normalization_factor * m_size_factor);
}