#pragma once
#include "AsyncComponent.h"
#include "PakalMath.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Pakal
{
	class SpriteComponent_Bgfx;

	// What a sprite draws instead of its quad: triangles over the texture (a polygon cut to the picture's outline, as an atlas
	// made by tools/pakal_atlas gives), so the transparent corners of a picture cost nothing to fill. The positions are in the
	// sprite's unit square as the quad is ((-0.5, -0.5) the bottom left, (0.5, 0.5) the top right, y up: the sprite's scale
	// still sizes it); the texture coordinates are in pixels of the texture, from its top left corner.
	struct SpriteMesh
	{
		std::vector<float> positions;    // x, y pairs
		std::vector<float> uvs;          // u, v pairs (pixels), one for each position
		std::vector<uint16_t> indices;   // three for each triangle
	};

	class _PAKALExport SpriteComponent2D : public AsyncComponent
	{
		DECLARE_RTTI_WITH_BASE(SpriteComponent2D, AsyncComponent);

	public:
		SpriteComponent2D() = default;
		virtual ~SpriteComponent2D() = default;

		virtual void set_texture(const std::string& texturePath) = 0;
		virtual void create_solid_color(uint32_t color, int width, int height) = 0;
		virtual void set_position(float x, float y) = 0;
		virtual tmath::vectorn<float, 2> get_position() const = 0;
		virtual void set_rotation(float radians) = 0;
		virtual void set_scale(float x, float y) = 0;
		virtual void set_color(float r, float g, float b, float a = 1.0f) = 0;
		virtual void set_layer(int layer) = 0;
		// Its place within its layer, higher in front: whole numbers, -32767 to 32767 (a y-sort passes minus its y in
		// centimetres). Used only when the game asked its graphics system for GraphicsSystem::SpriteOrder::LayerThenOrder;
		// otherwise sprites of one layer keep the order they have always had.
		virtual void set_order(float order) { (void)order; }
		virtual void set_visible(bool visible) = 0;
		virtual void set_flip(bool flip_x, bool flip_y) {}

		// The part of the texture the sprite shows, in pixels (an atlas frame without a mesh); the whole texture if never set
		virtual void set_texture_region(int x, int y, int width, int height) {}
		// Draws this mesh (copied) instead of the quad; null goes back to the quad
		virtual void set_mesh(const SpriteMesh* mesh) {}

		// The sprite's own looks, drawn by its shader (default no-ops):
		// a flash: the picture turned toward this color by 'amount' (0..1), over its tint (a hit's white flash)
		virtual void set_flash(float r, float g, float b, float amount) {}
		// a rim of light on the inside of its outline, 'width' texels wide (0: none); (dx, dy) where the light comes from in the
		// world (x right, y up; the sprite's flip is taken into account), (0, 0) all around
		virtual void set_rim(float r, float g, float b, float width, float dx = 0.0f, float dy = 0.0f) {}
		// another palette: the hues turned by 'turn' (0..1 of the circle), except near the hue 'keep' (0..1, within 'keepWidth':
		// the skin) and the greys; turn 0 is the picture's own
		virtual void set_hue(float turn, float keep = 0.07f, float keepWidth = 0.0f) {}
		// life over a still picture (a stage's background), painted in a mask the picture's size ('mask': another sprite of the same
		// scene showing it, hidden; it must outlive this one): red, water (its rows ripple sideways and it glints), green, what sways (leaves,
		// paper flags), blue, lights that flicker, alpha, water that runs down (a fountain's jets, a cascade); 'strength' scales it all (0: none). set_life_time: the seconds it animates by
		virtual void set_life(SpriteComponent2D* mask, float strength) {}
		virtual void set_life_time(float seconds) {}

		// Sprite sheet / animation support (default no-ops for implementations that don't support it)
		virtual void set_frame_size(int frame_width, int frame_height) {}
		virtual void set_frame(int frame) {}
		virtual void play_animation(int start_frame, int end_frame, bool loop = true) {}
		virtual void set_animation_fps(float fps) {}
	};
}
