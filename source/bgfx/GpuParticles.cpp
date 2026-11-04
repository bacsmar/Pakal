#include "GpuParticles.h"

#include <algorithm>
#include <cstring>

namespace Pakal
{
	void GpuParticles::create(const std::function<bgfx::ProgramHandle()>& simulate, const std::function<bgfx::ProgramHandle()>& draw,
		uint32_t capacity)
	{
		const uint64_t caps = bgfx::getCaps()->supported;
		m_simUniform = bgfx::createUniform("u_particleSim", bgfx::UniformType::Vec4);
		m_passUniform = bgfx::createUniform("u_gpuParticle", bgfx::UniformType::Vec4);
		m_simulate = simulate();
		m_draw = draw();
		if (!bgfx::isValid(m_simulate) || !bgfx::isValid(m_draw) || !(caps & BGFX_CAPS_COMPUTE) || !(caps & BGFX_CAPS_INSTANCING))
		{
			m_available = false;
			return;
		}
		m_capacity = capacity;
		bgfx::VertexLayout layout;
		layout.begin()
			.add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord1, 4, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord2, 4, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord3, 4, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float)
			.end();
		// all zeros: every particle starts dead (its age is not under its life)
		const bgfx::Memory* zeros = bgfx::alloc(capacity * kVec4s * 16);
		std::memset(zeros->data, 0, zeros->size);
		m_buffer = bgfx::createDynamicVertexBuffer(zeros, layout, BGFX_BUFFER_COMPUTE_READ_WRITE);
		bgfx::VertexLayout corner;
		corner.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
		static const float corners[] = { -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f };
		static const uint16_t indices[] = { 0, 1, 2, 0, 2, 3 };
		m_quad = bgfx::createVertexBuffer(bgfx::makeRef(corners, sizeof(corners)), corner);
		m_quadIndices = bgfx::createIndexBuffer(bgfx::makeRef(indices, sizeof(indices)));
		m_available = bgfx::isValid(m_buffer);
	}

	void GpuParticles::destroy()
	{
		if (bgfx::isValid(m_buffer)) bgfx::destroy(m_buffer);
		if (bgfx::isValid(m_quad)) bgfx::destroy(m_quad);
		if (bgfx::isValid(m_quadIndices)) bgfx::destroy(m_quadIndices);
		if (bgfx::isValid(m_simUniform)) bgfx::destroy(m_simUniform);
		if (bgfx::isValid(m_passUniform)) bgfx::destroy(m_passUniform);
		if (bgfx::isValid(m_simulate)) bgfx::destroy(m_simulate);
		if (bgfx::isValid(m_draw)) bgfx::destroy(m_draw);
		m_buffer = BGFX_INVALID_HANDLE;
		m_quad = BGFX_INVALID_HANDLE;
		m_quadIndices = BGFX_INVALID_HANDLE;
		m_simUniform = m_passUniform = BGFX_INVALID_HANDLE;
		m_simulate = m_draw = BGFX_INVALID_HANDLE;
		m_available = false;
	}

	void GpuParticles::emit(const Particle& p)
	{
		if (!m_available) return;
		const float data[kVec4s * 4] = {
			p.position[0], p.position[1], p.position[2], 0.0f,
			p.velocity[0], p.velocity[1], p.velocity[2], std::max(p.life, 0.001f),
			p.color[0], p.color[1], p.color[2], p.gravity,
			p.radius0, p.radius1, p.alpha0, p.alpha1,
			p.additive ? 1.0f : 0.0f, p.bounce ? 1.0f : 0.0f, 0.0f, 0.0f };
		std::lock_guard<std::mutex> lock(m_pendingMutex);
		if (m_pending.size() / (kVec4s * 4) >= m_capacity) return;   // more in one frame than the ring holds
		m_pending.insert(m_pending.end(), data, data + kVec4s * 4);
	}

	void GpuParticles::advance(float seconds)
	{
		std::lock_guard<std::mutex> lock(m_pendingMutex);
		m_driven = true;
		m_gameSeconds += std::max(seconds, 0.0f);
	}

	void GpuParticles::clear()
	{
		std::lock_guard<std::mutex> lock(m_pendingMutex);
		m_pending.clear();
		m_clear = true;
		m_driven = false;
		m_gameSeconds = 0.0f;
	}

	void GpuParticles::frame(float dt, bgfx::ViewId simulateView, bgfx::ViewId drawView)
	{
		if (!m_available) return;
		std::vector<float> fresh;
		bool clear = false;
		{
			std::lock_guard<std::mutex> lock(m_pendingMutex);
			fresh.swap(m_pending);
			if (m_driven)
			{
				dt = m_gameSeconds;
				m_gameSeconds = 0.0f;
			}
			clear = m_clear;
			m_clear = false;
		}
		if (clear)
		{
			// all zeros: every particle dead
			const bgfx::Memory* zeros = bgfx::alloc(m_capacity * kVec4s * 16);
			std::memset(zeros->data, 0, zeros->size);
			bgfx::update(m_buffer, 0, zeros);
			m_head = 0;
		}
		// the new ones into the ring from its head, in one piece or two where it wraps
		const uint32_t stride = kVec4s * 4;
		uint32_t count = static_cast<uint32_t>(fresh.size() / stride), done = 0;
		while (done < count)
		{
			const uint32_t run = std::min(count - done, m_capacity - m_head);
			bgfx::update(m_buffer, m_head, bgfx::copy(fresh.data() + done * stride, run * stride * sizeof(float)));
			m_head = (m_head + run) % m_capacity;
			done += run;
		}
		const float sim[4] = { std::clamp(dt, 0.0f, 0.1f), static_cast<float>(m_capacity), 0.0f, 0.0f };
		bgfx::setUniform(m_simUniform, sim);
		bgfx::setBuffer(0, m_buffer, bgfx::Access::ReadWrite);
		bgfx::dispatch(simulateView, m_simulate, (m_capacity + 63) / 64);
		// the blended ones, then the additive ones (light added in proportion to their alpha); depth tested, not written
		for (int additive = 0; additive < 2; ++additive)
		{
			const float pass[4] = { static_cast<float>(additive), 0.0f, 0.0f, 0.0f };
			bgfx::setUniform(m_passUniform, pass);
			bgfx::setVertexBuffer(0, m_quad);
			bgfx::setIndexBuffer(m_quadIndices);
			bgfx::setInstanceDataBuffer(m_buffer, 0, m_capacity);
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA
				| (additive ? BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE) : BGFX_STATE_BLEND_ALPHA));
			bgfx::submit(drawView, m_draw);
		}
	}
}
