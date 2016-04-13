////////////////////////////////////////////////////////////
//
//
////////////////////////////////////////////////////////////

#pragma once
#include <vector>
#include "PakalMath.h"
#include <memory>

namespace Pakal
{
	class TextReader;
	class Archive;

	class _PAKALExport Sprite
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

			void persist(Archive* archive);
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

		void persist(Archive* archive);
	};

	struct _PAKALExport SpriteSheet
	{
		std::vector<Sprite*> animations;	
		std::string texture_name;
		std::string default_animation;

		float	 meters_scale = 1;
		unsigned pixels_scale = 100;	// by default 100:1 (pixels:meters)

		void persist(Archive* archive);

		~SpriteSheet() 
		{
			for (auto animation : animations)
			{
				SAFE_DEL(animation);
			}
			animations.clear();
		}

		inline float get_length() const
		{
			return float(sqrt((pixels_scale * pixels_scale) << 1));
		}

		inline float get_scale() const
		{
			auto vlength = get_length();
			ASSERT(vlength != 0);
			return float((1.0 / vlength)*meters_scale);
		}

		bool load(TextReader& reader, std::istream& stream);
	};

	using SpriteSheetPtr = std::shared_ptr<SpriteSheet>;

}
