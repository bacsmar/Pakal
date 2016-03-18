////////////////////////////////////////////////////////////
//
//
////////////////////////////////////////////////////////////

#pragma once
#include "Sprite.h"
#include "persist/Archive.h"
#include "persist/TextReader.h"
#include "Utils.h"
#include "LogMgr.h"

namespace Pakal
{
	void Sprite::Frame::persist(Archive* archive)
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

	void Sprite::persist(Archive* archive)
	{
		archive->value("name", name);
		archive->value("looped", looped);
		archive->value("duration", duration);
		archive->value("", "frame", m_frames);
	}

	void SpriteSheet::persist(Archive* archive)
	{
		archive->value("texture", texture_name);
		archive->value("default", default_animation);
		archive->value("", "animation", animations);

		if( archive->get_type() == ArchiveType::Reader)
		{
			std::string scale;
			archive->value("scale", scale);
			auto tokens = Pakal::string_utils::split(scale, ":");

			bool error = tokens.size() != 2;
			ASSERT(error == false);
			if (error)
			{
				LOG_ERROR("[SpriteSheet Parser], undefined scale");
				return;
			}
			
			// scale = "pixels:meters"
			pixels_scale = std::stoul(tokens[0]);
			meters_scale = std::stof(tokens[1]);
		}

		else
		{
			auto scale = std::to_string(pixels_scale) + ":" + std::to_string(meters_scale);
			archive->value("scale", scale);
		}
	}

	bool SpriteSheet::load(TextReader& reader, std::istream& stream)
	{
		auto result = reader.read(stream, "SpriteSheetAnimation", *this);
		ASSERT(get_length() > 0);
		return result;
	}
}
