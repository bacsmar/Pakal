////////////////////////////////////////////////////////////
//
//
////////////////////////////////////////////////////////////

#pragma once
#include <vector>
#include "math/tm.h"

namespace Pakal
{
	class Sprite
	{
	public:

		inline void add_frame(Pakal::tmath::recti offset, Pakal::tmath::vector2di  relative_pos = { 0,0 })
		{
			m_frames.emplace_back(Frame{ offset, relative_pos });
		}
		inline std::size_t get_size() const
		{
			return m_frames.size();
		}
		inline const Pakal::tmath::recti& get_frame_offset(std::size_t index) const
		{
			return m_frames[index].frame_offset;
		}
		inline const Pakal::tmath::vector2di& get_frame_pos(std::size_t index) const
		{
			return m_frames[index].relative_pos;
		}

		//void set_sprite_sheet(irr::video::ITexture& texture);
		//irr::video::ITexture* get_sprite_sheet() const;

	private:
		struct Frame
		{
			Pakal::tmath::recti		frame_offset;
			Pakal::tmath::vector2di	relative_pos;
		};

		std::vector<Frame> m_frames;
		//irr::video::ITexture* m_texture;
	};
}