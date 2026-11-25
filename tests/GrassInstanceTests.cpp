// GrassInstanceTests: the GPU grass (source/bgfx/BgfxGrass) when the frame's instance buffer is nearly full. Every
// instanced draw of a frame shares that buffer, which bgfx grows by chunks, each taking a vertex buffer handle: with none
// left (a level of thousands of buffers) only the last chunk's room is had, and bgfx answers a request all or none. The
// grass took its tiles ring by ring and gave up at the first ring that did not fit whole, so such a frame drew none of a
// field, its nearest blades included. Now the tiles that fit are drawn, the nearest first, and the rest counted. bgfx runs its Noop renderer (no GPU, no window) with the build's compiled
// grass shaders (argument: their folder).
#include "bgfx/BgfxGrass.h"

#include <bgfx/bgfx.h>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace Pakal;

static int failures = 0;

static void check(bool condition, const char* what)
{
	std::printf("%s %s\n", condition ? "ok  " : "FAIL", what);
	if (!condition) ++failures;
}

static bgfx::ShaderHandle load_shader(const std::string& path)
{
	std::ifstream in(path, std::ios::binary);
	std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	if (bytes.empty()) return BGFX_INVALID_HANDLE;
	return bgfx::createShader(bgfx::copy(bytes.data(), uint32_t(bytes.size())));
}

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		std::printf("usage: GrassInstanceTests <folder of vs_grass.bin and fs_grass.bin>\n");
		return 1;
	}
	const std::string folder = argv[1];

	bgfx::renderFrame();   // before init: bgfx renders on this thread
	bgfx::Init init;
	init.type = bgfx::RendererType::Noop;
	if (!bgfx::init(init))
	{
		std::printf("FAIL bgfx's Noop renderer did not start\n");
		return 1;
	}

	{
		BgfxGrass grass;
		grass.create([&]() -> bgfx::ProgramHandle
		{
			const bgfx::ShaderHandle vs = load_shader(folder + "/vs_grass.bin"), fs = load_shader(folder + "/fs_grass.bin");
			if (!bgfx::isValid(vs) || !bgfx::isValid(fs)) return BGFX_INVALID_HANDLE;
			return bgfx::createProgram(vs, fs, true);
		});
		check(grass.available(), "the grass program is made from the build's shaders");

		// a 60 m field round the focus, no maps: every tile in reach is drawn
		GraphicsSystem::GrassField field;
		field.minX = -30.0f; field.maxX = 30.0f; field.minZ = -30.0f; field.maxZ = 30.0f;
		check(grass.add(1, field), "a field is made");
		BgfxGrass::Frame frame;   // the focus at the origin, no frustum, no sun's shadow
		const uint32_t white = 0xffffffff;
		frame.white = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, 0, bgfx::copy(&white, 4));
		const float eye[3] = { 0.0f, 10.0f, 10.0f };

		const uint32_t whole = grass.render(0, eye, 0.0, frame);
		check(whole > 0 && grass.dropped_tiles() == 0, "a frame with room draws the field whole");
		bgfx::frame();

		// a level that took every vertex buffer handle, and a frame whose other instanced draws left room for 3 tiles in the
		// instance buffer's last chunk (16 bytes a tile)
		bgfx::VertexLayout layout;
		layout.begin().add(bgfx::Attrib::Position, 1, bgfx::AttribType::Float).end();
		const float one = 0.0f;
		std::vector<bgfx::VertexBufferHandle> buffers;
		bgfx::InstanceDataBuffer first;
		bgfx::allocInstanceDataBuffer(&first, 1, 16);   // the frame's first chunk
		for (;;)
		{
			const bgfx::VertexBufferHandle vb = bgfx::createVertexBuffer(bgfx::copy(&one, sizeof(one)), layout);
			if (!bgfx::isValid(vb)) break;
			buffers.push_back(vb);
		}
		uint32_t room = 0, over = 1u << 26;   // the last chunk's room: bgfx answers all or none
		while (over - room > 1)
		{
			const uint32_t mid = room + (over - room) / 2;
			if (bgfx::getAvailInstanceDataBuffer(mid, 16) >= mid) room = mid; else over = mid;
		}
		check(room > 3 && bgfx::getAvailInstanceDataBuffer(room + 1, 16) == 0, "no handle left: only the last chunk's room");
		bgfx::InstanceDataBuffer taken;
		bgfx::allocInstanceDataBuffer(&taken, room - 3, 16);
		const uint32_t busy = grass.render(0, eye, 0.0, frame);
		check(busy > 0, "a busy frame still draws the tiles that fit");
		check(busy < whole, "fewer than a whole field");
		check(grass.dropped_tiles() > 0, "the tiles left out are counted");
		for (const auto vb : buffers) bgfx::destroy(vb);
		bgfx::frame();

		// the frame after has room again
		check(grass.render(0, eye, 0.0, frame) == whole && grass.dropped_tiles() == 0, "the next frame draws it whole again");
		bgfx::frame();
		grass.destroy();
		bgfx::destroy(frame.white);
	}
	bgfx::shutdown();
	std::printf(failures ? "%d failed\n" : "all passed\n", failures);
	return failures ? 1 : 0;
}
