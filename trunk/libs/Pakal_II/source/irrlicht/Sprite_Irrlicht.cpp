////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////

#include "Sprite_Irrlicht.hpp"

//core::recti;
//core::vector2di;

SpriteIrrlicht::SpriteIrrlicht() : m_texture(NULL)
{

}

void SpriteIrrlicht::addFrame(core::recti rect, core::vector2di offset)
{
	Frame frame = {rect, offset};
	m_frames.push_back(frame);    
}

void SpriteIrrlicht::setSpriteSheet(video::ITexture& texture)
{
    m_texture = &texture;
}

video::ITexture* SpriteIrrlicht::getSpriteSheet() const
{
    return m_texture;
}

std::size_t SpriteIrrlicht::getSize() const
{
    return m_frames.size();
}

const core::recti& SpriteIrrlicht::getFrame(std::size_t n) const
{    
	return m_frames[n].frame_offset;
}

const core::vector2di& SpriteIrrlicht::getOffset( std::size_t n ) const
{
	return m_frames[n].relative_pos;
}
