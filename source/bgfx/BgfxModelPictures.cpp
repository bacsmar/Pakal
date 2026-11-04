///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: BgfxModelPictures.cpp
// Purpose: BgfxGraphicsSystem's pictures of single models (an editor's asset browser): each drawn by
//          itself into a small target of its own, copied into a texture the CPU reads, and handed over
//          when bgfx has read it back; and the picture files and textures the development panels show.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "BgfxGraphicsSystem.h"
#include "LogMgr.h"
#include "MeshComponent3D_Bgfx.h"

#include <bimg/bimg.h>
#include <bimg/decode.h>
#include <bx/allocator.h>
#include <bx/file.h>
#include <bx/math.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>

namespace Pakal
{
	void BgfxGraphicsSystem::request_model_picture(MeshComponent3D_Bgfx* mesh, uint32_t size, PictureDone done)
	{
		const bgfx::Caps* caps = bgfx::getCaps();
		const bool canRead = caps && (caps->supported & BGFX_CAPS_TEXTURE_BLIT) && (caps->supported & BGFX_CAPS_TEXTURE_READ_BACK);
		if (!mesh || !mesh->is_loaded() || size == 0 || !canRead)
		{
			if (done) done(size, {});
			return;
		}
		PictureJob job;
		job.mesh = mesh;
		job.size = std::min<uint32_t>(size, 1024);
		job.done = std::move(done);
		m_pictureJobs.push_back(std::move(job));
	}

	void BgfxGraphicsSystem::release_picture(PictureJob& job)
	{
		if (bgfx::isValid(job.target)) bgfx::destroy(job.target);   // it owns its color and depth textures
		if (bgfx::isValid(job.readback)) bgfx::destroy(job.readback);
		job.target = BGFX_INVALID_HANDLE;
		job.color = job.depth = job.readback = BGFX_INVALID_HANDLE;
	}

	void BgfxGraphicsSystem::draw_model_picture()
	{
		// the first one not drawn yet (one each frame: they share the view)
		auto it = std::find_if(m_pictureJobs.begin(), m_pictureJobs.end(), [](const PictureJob& job) { return job.readyFrame == 0; });
		if (it == m_pictureJobs.end()) return;
		PictureJob& job = *it;
		const uint16_t size = static_cast<uint16_t>(job.size);
		const uint64_t sampling = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
		job.color = bgfx::createTexture2D(size, size, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT | sampling);
		job.depth = bgfx::createTexture2D(size, size, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT_WRITE_ONLY);
		const bgfx::TextureHandle attachments[] = { job.color, job.depth };
		job.target = bgfx::createFrameBuffer(2, attachments, true);
		job.readback = bgfx::createTexture2D(size, size, false, 1, bgfx::TextureFormat::RGBA8,
			BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK | sampling);
		if (!bgfx::isValid(job.target) || !bgfx::isValid(job.readback))
		{
			LOG_WARNING("[BgfxGraphicsSystem] a model's picture could not be made");
			release_picture(job);
			job.readyFrame = ~0u;   // handed over empty below
			return;
		}

		// framed whole: the camera on the model's box from the south-east and above, far enough for its bounding sphere
		float low[3], high[3];
		job.mesh->get_bounds(low, high);
		const float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
		job.mesh->set_matrix(identity);
		const bx::Vec3 center = { (low[0] + high[0]) * 0.5f, (low[1] + high[1]) * 0.5f, (low[2] + high[2]) * 0.5f };
		const float radius = std::max(0.05f, 0.5f * std::sqrt((high[0] - low[0]) * (high[0] - low[0]) + (high[1] - low[1]) * (high[1] - low[1])
			+ (high[2] - low[2]) * (high[2] - low[2])));
		const float fovY = 30.0f;
		const float distance = radius / std::sin(bx::toRad(fovY * 0.5f)) * 1.02f;
		const bx::Vec3 toward = bx::normalize(bx::Vec3{ 0.75f, 0.62f, 1.0f });
		const bx::Vec3 eye = bx::add(center, bx::mul(toward, distance));
		float view[16], projection[16];
		bx::mtxLookAt(view, eye, center, { 0.0f, 1.0f, 0.0f }, bx::Handedness::Right);
		bx::mtxProj(projection, fovY, 1.0f, std::max(0.01f, distance - radius * 1.5f), distance + radius * 1.5f, bgfx::getCaps()->homogeneousDepth,
			bx::Handedness::Right);

		const bgfx::ViewId view_ = m_picture_view_id;
		bgfx::setViewName(view_, "model picture");
		bgfx::setViewRect(view_, 0, 0, size, size);
		bgfx::setViewFrameBuffer(view_, job.target);
		bgfx::setViewClear(view_, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x00000000, 1.0f, 0);
		bgfx::setViewTransform(view_, view, projection);
		bgfx::setViewMode(view_, bgfx::ViewMode::Sequential);
		bgfx::touch(view_);
		// a fixed sun over the right shoulder, and an even sky: every picture lit the same, whatever the level's look
		const float lightDir[4] = { -0.45f, -1.0f, -0.35f, 0.0f };
		const float lightColor[4] = { 1.0f, 0.97f, 0.9f, 1.0f };
		const float ambient[4] = { 0.42f, 0.43f, 0.48f, 1.0f };
		const float ambientGround[4] = { 0.28f, 0.27f, 0.26f, 1.0f };
		bgfx::setUniform(m_u_light_dir, lightDir);
		bgfx::setUniform(m_u_light_color, lightColor);
		bgfx::setUniform(m_u_ambient, ambient);
		bgfx::setUniform(m_u_ambient_ground, ambientGround);
		const bgfx::ProgramHandle program = job.mesh->is_skinned() ? m_skin_program
			: (job.mesh->uses_pbr() && bgfx::isValid(m_pbr_program) ? m_pbr_program : m_mesh_program);
		job.mesh->render_alone(view_, program);

		// copied for the CPU after it is drawn, in the view after it
		const bgfx::ViewId copy = static_cast<bgfx::ViewId>(m_picture_view_id + 1);
		bgfx::touch(copy);
		bgfx::blit(copy, job.readback, 0, 0, job.color);
		job.pixels.assign(size_t(size) * size * 4, 0);
		job.readyFrame = bgfx::readTexture(job.readback, job.pixels.data());
	}

	void BgfxGraphicsSystem::finish_model_pictures()
	{
		for (size_t i = 0; i < m_pictureJobs.size();)
		{
			PictureJob& job = m_pictureJobs[i];
			if (job.readyFrame == 0 || (job.readyFrame != ~0u && m_frameNumber < job.readyFrame))
			{
				++i;
				continue;
			}
			std::vector<uint8_t> pixels;
			if (job.readyFrame != ~0u)
			{
				pixels = std::move(job.pixels);
				if (bgfx::getCaps()->originBottomLeft)
				{
					// rows from the top
					const size_t row = size_t(job.size) * 4;
					for (size_t y = 0; y < job.size / 2; ++y)
					{
						std::swap_ranges(pixels.begin() + std::ptrdiff_t(y * row), pixels.begin() + std::ptrdiff_t((y + 1) * row),
							pixels.begin() + std::ptrdiff_t((job.size - 1 - y) * row));
					}
				}
			}
			release_picture(job);
			PictureDone done = std::move(job.done);
			const uint32_t size = job.size;
			m_pictureJobs.erase(m_pictureJobs.begin() + std::ptrdiff_t(i));
			if (done) done(size, pixels);   // it may ask for another (pushed at the end)
		}
	}

	bgfx::TextureHandle BgfxGraphicsSystem::create_ui_texture(uint32_t width, uint32_t height, const std::vector<uint8_t>& rgba)
	{
		if (width == 0 || height == 0 || rgba.size() < size_t(width) * height * 4) return BGFX_INVALID_HANDLE;
		return bgfx::createTexture2D(static_cast<uint16_t>(width), static_cast<uint16_t>(height), false, 1, bgfx::TextureFormat::RGBA8,
			BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, bgfx::copy(rgba.data(), static_cast<uint32_t>(size_t(width) * height * 4)));
	}

	void BgfxGraphicsSystem::destroy_ui_texture(bgfx::TextureHandle texture)
	{
		if (bgfx::isValid(texture)) bgfx::destroy(texture);
	}

	bool BgfxGraphicsSystem::write_png(const std::string& path, uint32_t width, uint32_t height, const std::vector<uint8_t>& rgba)
	{
		if (width == 0 || height == 0 || rgba.size() < size_t(width) * height * 4) return false;
		bx::FileWriter writer;
		bx::Error error;
		if (!bx::open(&writer, bx::FilePath(path.c_str()), false, &error)) return false;
		bimg::imageWritePng(&writer, width, height, width * 4, rgba.data(), bimg::TextureFormat::RGBA8, false, &error);
		bx::close(&writer);
		return error.isOk();
	}

	bool BgfxGraphicsSystem::read_image(const std::string& path, uint32_t& width, uint32_t& height, std::vector<uint8_t>& rgba)
	{
		std::ifstream in(path, std::ios::binary);
		if (!in) return false;
		const std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		if (bytes.empty()) return false;
		static bx::DefaultAllocator allocator;
		bimg::ImageContainer* image = bimg::imageParse(&allocator, bytes.data(), static_cast<uint32_t>(bytes.size()), bimg::TextureFormat::RGBA8);
		if (!image) return false;
		width = image->m_width;
		height = image->m_height;
		const uint8_t* data = static_cast<const uint8_t*>(image->m_data);
		rgba.assign(data, data + size_t(width) * height * 4);
		bimg::imageFree(image);
		return true;
	}
}
