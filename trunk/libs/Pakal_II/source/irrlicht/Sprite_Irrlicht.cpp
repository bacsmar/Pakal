////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

#include "Sprite_Irrlicht.hpp"

//core::recti;
//core::vector2di;

SpriteIrrlicht::SpriteIrrlicht() : m_texture(NULL)
{

}

void SpriteIrrlicht::add_frame(core::recti rect, core::vector2di offset)
{
	Frame frame = {rect, offset};
	m_frames.push_back(frame);    
}

void SpriteIrrlicht::set_sprite_sheet(video::ITexture& texture)
{
    m_texture = &texture;
}

video::ITexture* SpriteIrrlicht::get_sprite_sheet() const
{
    return m_texture;
}

std::size_t SpriteIrrlicht::get_size() const
{
    return m_frames.size();
}

const core::recti& SpriteIrrlicht::get_frame(std::size_t n) const
{    
	return m_frames[n].frame_offset;
}

const core::vector2di& SpriteIrrlicht::get_offset( std::size_t n ) const
{
	return m_frames[n].relative_pos;
}
