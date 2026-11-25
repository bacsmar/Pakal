#pragma once
// The offscreen targets the 3D scene is drawn into: the post-process target (its depth read by the post pass) and the
// upscale target (its depth only written). One place makes both, so SceneTargetTests checks the same flags the engine
// uses (a depth attachment created with bgfx's default attachment flag is refused on every GPU).
#include <bgfx/bgfx.h>
#include <cstdint>

namespace Pakal
{
	namespace SceneTarget
	{
		// The post pass reads the depth with a point sampler (depth is not filtered)
		constexpr uint64_t kPostDepthFlags = BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT
			| BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
		// The upscale target's depth is only drawn into
		constexpr uint64_t kUpscaleDepthFlags = BGFX_TEXTURE_RT_WRITE_ONLY;

		enum class Failure { None, ColorFormat, DepthFormat, Texture, FrameBuffer };

		struct Result
		{
			bgfx::FrameBufferHandle target = BGFX_INVALID_HANDLE;
			bgfx::TextureHandle color = BGFX_INVALID_HANDLE;
			bgfx::TextureHandle depth = BGFX_INVALID_HANDLE;
			Failure failure = Failure::None;
		};

		// A color and a depth texture of width x height and the frame buffer over them (which owns them). flags: the
		// render target's (BGFX_TEXTURE_RT or an MSAA one), on both; depthFlags: added to the depth's. Nothing is left
		// created when it fails, and 'failure' says which step did.
		inline Result create(uint16_t width, uint16_t height, bgfx::TextureFormat::Enum colorFormat,
			bgfx::TextureFormat::Enum depthFormat, uint64_t flags, uint64_t depthFlags)
		{
			Result r;
			const uint64_t colorFlags = flags | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
			if (!bgfx::isTextureValid(0, false, 1, colorFormat, colorFlags))
			{
				r.failure = Failure::ColorFormat;
				return r;
			}
			if (!bgfx::isTextureValid(0, false, 1, depthFormat, flags | depthFlags))
			{
				r.failure = Failure::DepthFormat;
				return r;
			}
			r.color = bgfx::createTexture2D(width, height, false, 1, colorFormat, colorFlags);
			r.depth = bgfx::createTexture2D(width, height, false, 1, depthFormat, flags | depthFlags);
			r.failure = Failure::Texture;
			if (bgfx::isValid(r.color) && bgfx::isValid(r.depth))
			{
				bgfx::Attachment attachments[2];
				// no flags: init's default (auto-generated mips) is refused on a depth attachment, and the targets have no mips
				attachments[0].init(r.color, bgfx::Access::Write, 0, 1, 0, BGFX_ATTACHMENT_NONE);
				attachments[1].init(r.depth, bgfx::Access::Write, 0, 1, 0, BGFX_ATTACHMENT_NONE);
				r.failure = Failure::FrameBuffer;
				if (bgfx::isFrameBufferValid(2, attachments))
				{
					r.target = bgfx::createFrameBuffer(2, attachments, true);
					if (bgfx::isValid(r.target)) r.failure = Failure::None;
				}
			}
			if (!bgfx::isValid(r.target))
			{
				if (bgfx::isValid(r.color)) bgfx::destroy(r.color);
				if (bgfx::isValid(r.depth)) bgfx::destroy(r.depth);
				r.color = r.depth = BGFX_INVALID_HANDLE;
			}
			return r;
		}

		inline const char* failure_name(Failure failure)
		{
			switch (failure)
			{
			case Failure::None: return "none";
			case Failure::ColorFormat: return "its color format";
			case Failure::DepthFormat: return "its depth format";
			case Failure::Texture: return "a texture";
			case Failure::FrameBuffer: return "the frame buffer";
			}
			return "?";
		}
	}
}
