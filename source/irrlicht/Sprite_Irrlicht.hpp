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

	void addFrame(irr::core::recti rect, irr::core::vector2di  offset = core::vector2di(0,0) );	
    void setSpriteSheet(video::ITexture& texture);
    video::ITexture* getSpriteSheet() const;
    std::size_t getSize() const;
    const core::recti& getFrame(std::size_t n) const;
	const core::vector2di& getOffset(std::size_t n) const;

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