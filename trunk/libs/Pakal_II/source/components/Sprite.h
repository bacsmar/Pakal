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

		// add a frame rect to animation
		inline void add_frame(Pakal::tmath::recti offset, Pakal::tmath::vector2df  relative_pos = { 0,0 })
		{
			m_frames.emplace_back(Frame{ offset, relative_pos });
		}
		// get number of frames
		inline std::size_t get_size() const
		{
			return m_frames.size();
		}
		// 
		inline const Pakal::tmath::recti& get_frame_offset(std::size_t index) const
		{
			return m_frames[index].frame_offset;
		}
		//
		inline const Pakal::tmath::vector2df& get_frame_pos(std::size_t index) const
		{
			return m_frames[index].relative_pos;
		}		
		// frame_time = frame_count/duration
		unsigned	duration = 100;
		bool		is_looped = false;

	private:
		struct Frame
		{
			Pakal::tmath::recti		frame_offset;
			Pakal::tmath::vector2df	relative_pos;
		};

		std::vector<Frame> m_frames;		
	};
}