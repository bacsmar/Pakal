////////////////////////////////////////////////////////////
//
//
////////////////////////////////////////////////////////////

#pragma once
#include <vector>
#include "PakalMath.h"
#include "persist/Archive.h"
#include "persist/TextReader.h"

namespace Pakal
{
	class Sprite
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
		unsigned	duration = 600;
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
			archive->value("", "frame", m_frames);
		}

	};

	struct SpriteSheet
	{
		std::string texture_name;
		std::string default_animation;
		float size_factor = 1;

		tmath::vector2du m_ref_size;
		// why is not a good idea to put a ref_animation?. instead of ref_size...
		// because the sprite sheet resizes every sprite on it

		std::vector<Sprite*> animations;

		void persist(Archive* archive)
		{
			archive->value("refHeight", m_ref_size.x);
			archive->value("refWidth", m_ref_size.y);
			archive->value("size_factor", size_factor);
			archive->value("texture", texture_name);
			archive->value("default", default_animation);
			archive->value("", "animation", animations);
		}

		inline tmath::vector2df get_scale() const
		{
			auto vlength = m_ref_size.get_length();
			ASSERT(vlength != 0);
			return 1.f / vlength;
		}

		bool load(TextReader& reader, std::istream& stream)
		{
			auto result = reader.read(stream, "SpriteSheetAnimation", *this);
			ASSERT(m_ref_size.get_length() > 0);
			return result;
		}
	};

}
