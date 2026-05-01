#pragma once
#include "Component.h"
#include "PakalMath.h"
#include <string>

namespace Pakal
{
	class SpriteComponent_Bgfx;

	class _PAKALExport SpriteComponent2D : public Component
	{
		DECLARE_RTTI_WITH_BASE(SpriteComponent2D, Component);

	public:
		SpriteComponent2D() = default;
		virtual ~SpriteComponent2D() = default;

		virtual void set_texture(const std::string& texturePath) = 0;
		virtual void create_solid_color(uint32_t color, int width, int height) = 0;
		virtual void set_position(float x, float y) = 0;
		virtual void set_rotation(float radians) = 0;
		virtual void set_scale(float x, float y) = 0;
		virtual void set_color(float r, float g, float b, float a = 1.0f) = 0;
		virtual void set_layer(int layer) = 0;
		virtual void set_visible(bool visible) = 0;

		// Sprite sheet / animation support (default no-ops for implementations that don't support it)
		virtual void set_frame_size(int frame_width, int frame_height) {}
		virtual void set_frame(int frame) {}
		virtual void play_animation(int start_frame, int end_frame, bool loop = true) {}
		virtual void set_animation_fps(float fps) {}
	};
}
