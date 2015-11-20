////////////////////////////////////////////////////////////
//
//
////////////////////////////////////////////////////////////

#pragma once
#include <vector>
#include "math/tm.h"
#include "persist/Archive.h"

namespace Pakal
{

	class SpriteAnimation
	{
		struct Frame
		{
			tmath::recti		texture_rect;
			tmath::vector2df	offset;
			tmath::vector2df	pivot;

			inline tmath::vector2df relative_pos() const
			{
				return offset - pivot;
			}

			void persist(Archive* archive)
			{

				archive->value("x", texture_rect.left_corner.x);
				archive->value("y", texture_rect.left_corner.y);

				archive->value("w", texture_rect.size.x);
				archive->value("h", texture_rect.size.y);

				archive->value("oX", offset.x);
				archive->value("oY", offset.y);

				archive->value("pX", pivot.x);
				archive->value("pY", pivot.y);
			}
		};

		std::vector<Frame> m_frames;

	public:

		std::string name;
		unsigned	duration = 100;
		bool		looped = false;
		
		inline void add_frame(tmath::recti rect, tmath::vector2df  offset = { 0,0 })
		{
			m_frames.emplace_back(Frame{ rect, offset });
		}

		inline size_t get_size() const
		{
			return m_frames.size();
		}

		inline const Frame& get_frame(size_t index) const
		{
			return m_frames[index];
		}

		void persist(Archive* archive)
		{
			archive->value("name", name);
			archive->value("looped", looped);
			archive->value("duration", duration);
			archive->value("","frame",m_frames);
		}

	};

	struct SpriteLoader
	{
		std::string texture_name;
		std::string default_animation;
		unsigned ref_width = 0;
		unsigned ref_height = 0;
		float size_factor = 1;

		std::vector<SpriteAnimation*> animations;

		void persist(Archive* archive)
		{
			archive->value("refHeight", ref_height);
			archive->value("refWidth", ref_width);
			archive->value("size_factor", size_factor);
			archive->value("texture", texture_name);
			archive->value("default", default_animation);
			archive->value("","animation",animations);
		}
	};

}