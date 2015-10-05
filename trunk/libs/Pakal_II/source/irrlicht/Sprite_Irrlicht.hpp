////////////////////////////////////////////////////////////
//
//
////////////////////////////////////////////////////////////

#ifndef IRR_ANIMATION_INCLUDE
#define IRR_ANIMATION_INCLUDE

#include <vector>
#include "irrlicht.h"

using namespace irr;

class SpriteIrrlicht
{
public:
    SpriteIrrlicht();

	void add_frame(irr::core::recti rect, irr::core::vector2di  offset = core::vector2di(0,0) );	
    void set_sprite_sheet(video::ITexture& texture);
    video::ITexture* get_sprite_sheet() const;
    std::size_t get_size() const;
    const core::recti& get_frame(std::size_t n) const;
	const core::vector2di& get_offset(std::size_t n) const;

private:
	struct Frame
	{
		core::recti		frame_offset;
		core::vector2di	relative_pos;
	};
    
	std::vector<Frame> m_frames;
    video::ITexture* m_texture;
};

#endif // IRR_ANIMATION_INCLUDE