// The scene's offscreen targets (source/bgfx/BgfxSceneTarget.h) are accepted by bgfx's own validation, with the formats
// and flags the post-process and the upscale use. bgfx runs its Noop renderer, which supports every format: what fails
// here is the engine's choice of flags, never the GPU. (A depth attachment with bgfx's default flag,
// auto-generated mips, is refused on every GPU: every post and upscale target failed, and a scaled picture was drawn
// in a corner of the screen.)
#include "bgfx/BgfxSceneTarget.h"

#include <cstdio>

using namespace Pakal;

static int failures = 0;

static void expect_made(const char* what, bgfx::TextureFormat::Enum color, bgfx::TextureFormat::Enum depth, uint64_t flags,
	uint64_t depthFlags)
{
	const SceneTarget::Result made = SceneTarget::create(320, 180, color, depth, flags, depthFlags);
	if (made.failure != SceneTarget::Failure::None || !bgfx::isValid(made.target))
	{
		std::printf("FAIL %s: not made (%s)\n", what, SceneTarget::failure_name(made.failure));
		++failures;
		return;
	}
	std::printf("ok   %s\n", what);
	bgfx::destroy(made.target);   // it owns its textures
}

int main()
{
	bgfx::renderFrame();   // before init: bgfx renders on this thread, no render thread to wait for
	bgfx::Init init;
	init.type = bgfx::RendererType::Noop;
	if (!bgfx::init(init))
	{
		std::printf("FAIL bgfx's Noop renderer did not start\n");
		return 1;
	}

	using F = bgfx::TextureFormat;
	// the post-process target: its depth read by the post pass, in each format prepare_post_target tries
	const F::Enum postDepths[] = { F::D32F, F::D24S8, F::D24 };
	for (F::Enum depth : postDepths)
	{
		char what[64];
		std::snprintf(what, sizeof(what), "post target, depth %d", int(depth));
		expect_made(what, F::BGRA8, depth, BGFX_TEXTURE_RT, SceneTarget::kPostDepthFlags);
	}
	// the upscale target (the picture drawn smaller and scaled up to the screen)
	expect_made("upscale target", F::BGRA8, F::D24S8, BGFX_TEXTURE_RT, SceneTarget::kUpscaleDepthFlags);
	// the upscale target with cutout MSAA (no post-process)
	expect_made("upscale target, 4x MSAA", F::RGBA8, F::D24S8, BGFX_TEXTURE_RT_MSAA_X4, SceneTarget::kUpscaleDepthFlags);

	bgfx::frame();
	bgfx::shutdown();
	std::printf(failures ? "%d failed\n" : "all passed\n", failures);
	return failures ? 1 : 0;
}
