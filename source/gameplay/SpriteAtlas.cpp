///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SpriteAtlas.cpp
// Purpose: A texture atlas made by tools/pakal_atlas - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "gameplay/SpriteAtlas.h"

#include <nlohmann/json.hpp>

#include <algorithm>

namespace Pakal
{
	int AtlasAnimation::total_ms() const
	{
		int total = 0;
		for (int t : ms) total += std::max(1, t);
		return std::max(1, total);
	}

	int AtlasAnimation::index_at(double elapsedMs) const
	{
		if (frames.empty()) return 0;
		double t = std::max(0.0, elapsedMs);
		if (loop)
		{
			const double total = total_ms();
			t -= total * static_cast<double>(static_cast<long long>(t / total));
		}
		for (size_t i = 0; i < frames.size(); ++i)
		{
			const double length = std::max(1, i < ms.size() ? ms[i] : 100);
			if (t < length) return static_cast<int>(i);
			t -= length;
		}
		return static_cast<int>(frames.size()) - 1;
	}

	namespace
	{
		bool fail(std::string* error, const std::string& why)
		{
			if (error) *error = why;
			return false;
		}
	}

	bool SpriteAtlas::load_json(const std::string& text, std::string* error)
	{
		const nlohmann::json root = nlohmann::json::parse(text, nullptr, false);
		if (!root.is_object() || root.value("format", std::string()) != "pakal-atlas")
			return fail(error, "not a pakal-atlas file");
		if (root.value("version", 0) != 1)
			return fail(error, "a pakal-atlas of a version this engine does not read");
		try
		{
			SpriteAtlas atlas;
			atlas.m_image = root.value("image", std::string());
			const auto size = root.value("size", std::vector<int>{ 0, 0 });
			atlas.m_width = size.size() > 0 ? size[0] : 0;
			atlas.m_height = size.size() > 1 ? size[1] : 0;
			atlas.m_pixelsPerMeter = root.value("pixelsPerMeter", 100.0f);
			if (atlas.m_width <= 0 || atlas.m_height <= 0 || atlas.m_pixelsPerMeter <= 0.0f) return fail(error, "the atlas has no size");

			const nlohmann::json frames = root.value("frames", nlohmann::json::object());
			for (const auto& [name, f] : frames.items())
			{
				AtlasFrame frame;
				frame.name = name;
				const auto rect = f.value("rect", std::vector<int>());
				const auto offset = f.value("offset", std::vector<int>{ 0, 0 });
				const auto source = f.value("source", std::vector<int>());
				if (rect.size() != 4 || source.size() != 2 || offset.size() != 2) return fail(error, "frame '" + name + "' has no rect, offset or source");
				frame.x = rect[0]; frame.y = rect[1]; frame.width = rect[2]; frame.height = rect[3];
				frame.offsetX = offset[0]; frame.offsetY = offset[1];
				frame.sourceWidth = source[0]; frame.sourceHeight = source[1];
				if (frame.width <= 0 || frame.height <= 0 || frame.sourceWidth <= 0 || frame.sourceHeight <= 0 ||
					frame.x < 0 || frame.y < 0 || frame.x + frame.width > atlas.m_width || frame.y + frame.height > atlas.m_height)
					return fail(error, "frame '" + name + "' is outside the atlas or has no size");
				const auto pivot = f.value("pivot", std::vector<float>{ 0.5f, 0.5f });
				if (pivot.size() == 2) { frame.pivotX = pivot[0]; frame.pivotY = pivot[1]; }

				// the polygon (pixels of the trimmed picture, from its top left) or, without one, its rectangle
				std::vector<std::vector<float>> vertices;
				std::vector<int> triangles;
				if (f.contains("vertices"))
				{
					vertices = f["vertices"].get<std::vector<std::vector<float>>>();
					triangles = f.value("triangles", std::vector<int>());
					if (triangles.empty() || triangles.size() % 3 != 0) return fail(error, "frame '" + name + "' has a mesh without triangles");
					for (int t : triangles)
						if (t < 0 || t >= static_cast<int>(vertices.size())) return fail(error, "frame '" + name + "' has a triangle past its vertices");
					for (const auto& v : vertices)
						if (v.size() != 2) return fail(error, "frame '" + name + "' has a vertex that is not x, y");
				}
				else
				{
					const float w = static_cast<float>(frame.width), h = static_cast<float>(frame.height);
					vertices = { { 0.0f, 0.0f }, { w, 0.0f }, { w, h }, { 0.0f, h } };
					triangles = { 0, 1, 2, 0, 2, 3 };
				}
				for (const auto& v : vertices)
				{
					const float sx = static_cast<float>(frame.offsetX) + v[0], sy = static_cast<float>(frame.offsetY) + v[1];
					frame.mesh.positions.push_back(sx / static_cast<float>(frame.sourceWidth) - frame.pivotX);
					frame.mesh.positions.push_back(frame.pivotY - sy / static_cast<float>(frame.sourceHeight));
					frame.mesh.uvs.push_back(static_cast<float>(frame.x) + v[0]);
					frame.mesh.uvs.push_back(static_cast<float>(frame.y) + v[1]);
				}
				for (int t : triangles) frame.mesh.indices.push_back(static_cast<uint16_t>(t));
				atlas.m_frameIndex[name] = static_cast<int>(atlas.m_frames.size());
				atlas.m_frames.push_back(std::move(frame));
			}

			const nlohmann::json animations = root.value("animations", nlohmann::json::object());
			for (const auto& [name, a] : animations.items())
			{
				AtlasAnimation animation;
				animation.name = name;
				for (const auto& frameName : a.value("frames", std::vector<std::string>()))
				{
					auto found = atlas.m_frameIndex.find(frameName);
					if (found == atlas.m_frameIndex.end()) return fail(error, "animation '" + name + "' uses the frame '" + frameName + "', which is not in the atlas");
					animation.frames.push_back(found->second);
				}
				animation.ms = a.value("ms", std::vector<int>());
				animation.ms.resize(animation.frames.size(), animation.ms.empty() ? 100 : animation.ms.back());
				animation.loop = a.value("loop", false);
				atlas.m_animations[name] = std::move(animation);
			}
			*this = std::move(atlas);
		}
		catch (const nlohmann::json::exception& e)
		{
			return fail(error, std::string("not a pakal-atlas: ") + e.what());
		}
		return true;
	}

	const AtlasFrame* SpriteAtlas::frame(const std::string& name) const
	{
		auto found = m_frameIndex.find(name);
		return found == m_frameIndex.end() ? nullptr : &m_frames[found->second];
	}

	const AtlasAnimation* SpriteAtlas::animation(const std::string& name) const
	{
		auto found = m_animations.find(name);
		return found == m_animations.end() ? nullptr : &found->second;
	}

	const AtlasFrame* SpriteAtlas::frame_at(const AtlasAnimation& animation, double elapsedMs) const
	{
		if (animation.frames.empty()) return nullptr;
		return &m_frames[animation.frames[animation.index_at(elapsedMs)]];
	}

	void SpriteAtlas::size_in_meters(const AtlasFrame& frame, float& width, float& height) const
	{
		width = static_cast<float>(frame.sourceWidth) / m_pixelsPerMeter;
		height = static_cast<float>(frame.sourceHeight) / m_pixelsPerMeter;
	}

	void SpriteAtlas::show(SpriteComponent2D& sprite, const AtlasFrame& frame) const
	{
		float w, h;
		size_in_meters(frame, w, h);
		sprite.set_scale(w, h);
		sprite.set_mesh(&frame.mesh);
	}
}
