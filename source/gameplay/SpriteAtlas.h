///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SpriteAtlas.h
//
// Purpose: A texture atlas made by tools/pakal_atlas (docs/ATLAS.md): named frames (each a trimmed picture, with its place in
//          the untrimmed frame, its pivot and a mesh cut to its outline) and the animations that string them together
//          (frames, the time of each, loop). The atlas reads its JSON; a sprite is given a frame with show(): its mesh, and
//          the size of the untrimmed frame in meters, so the sprite's centre is the frame's pivot whatever was trimmed.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Platform.h"
#include "components/SpriteComponent2D.h"

#include <map>
#include <string>
#include <vector>

namespace Pakal
{
	struct AtlasFrame
	{
		std::string name;
		int x = 0, y = 0, width = 1, height = 1;      // the trimmed picture in the atlas (pixels)
		int offsetX = 0, offsetY = 0;                 // where it sits in the untrimmed frame (from its top left)
		int sourceWidth = 1, sourceHeight = 1;        // the untrimmed frame
		float pivotX = 0.5f, pivotY = 0.5f;           // in the untrimmed frame, 0..1: the sprite's centre
		SpriteMesh mesh;                              // made by the atlas: around the pivot, texture coordinates in atlas pixels
	};

	struct AtlasAnimation
	{
		std::string name;
		std::vector<int> frames;                      // indices into the atlas's frames
		std::vector<int> ms;                          // of each
		bool loop = false;

		int total_ms() const;
		// The index (into 'frames') showing 'elapsedMs' after the start: a loop goes round, one that plays once holds its last
		int index_at(double elapsedMs) const;
	};

	class _PAKALExport SpriteAtlas
	{
	public:
		// False (and a reason in 'error') if the text is not an atlas of this format and version, or a frame or an animation
		// does not make sense (a mesh that points past its vertices, an animation of a frame that does not exist)
		bool load_json(const std::string& text, std::string* error = nullptr);

		// The picture's file, as the JSON names it (relative to the JSON), and its size
		const std::string& image() const { return m_image; }
		int width() const { return m_width; }
		int height() const { return m_height; }
		float pixels_per_meter() const { return m_pixelsPerMeter; }

		const std::vector<AtlasFrame>& frames() const { return m_frames; }
		const AtlasFrame* frame(const std::string& name) const;
		const AtlasAnimation* animation(const std::string& name) const;
		const std::map<std::string, AtlasAnimation>& animations() const { return m_animations; }
		// The frame of an animation at a time
		const AtlasFrame* frame_at(const AtlasAnimation& animation, double elapsedMs) const;

		// The size of a frame's untrimmed picture in meters (what the sprite is scaled to)
		void size_in_meters(const AtlasFrame& frame, float& width, float& height) const;
		// Gives the sprite this frame (its texture must be the atlas's picture): the mesh and the size
		void show(SpriteComponent2D& sprite, const AtlasFrame& frame) const;

	private:
		std::string m_image;
		int m_width = 0, m_height = 0;
		float m_pixelsPerMeter = 100.0f;
		std::vector<AtlasFrame> m_frames;
		std::map<std::string, int> m_frameIndex;
		std::map<std::string, AtlasAnimation> m_animations;
	};
}
