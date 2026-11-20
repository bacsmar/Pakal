///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ParticleEffects.cpp
// Purpose: Particle effects made of data - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/view/ParticleEffects.h"

#include "IParticleRenderer.h"
#include "LogMgr.h"
#include "ResourceManager.h"

#include <algorithm>
#include <cmath>

namespace Pakal::TopDown
{
	namespace
	{
		const float kDegrees = 0.0174532925f;
		const float kFloorLift = 0.02f;   // a landed particle or a stain just over the floor

		uint32_t abgr(const std::array<float, 4>& c)
		{
			auto byte = [](float v) { return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
			return (byte(c[3]) << 24) | (byte(c[2]) << 16) | (byte(c[1]) << 8) | byte(c[0]);
		}
	}

	float ParticleEffects::Curve::at(float t) const
	{
		if (points.empty()) return 1.0f;
		if (t <= points.front()[0]) return points.front()[1];
		for (size_t i = 1; i < points.size(); ++i)
		{
			if (t <= points[i][0])
			{
				const float span = points[i][0] - points[i - 1][0];
				const float k = span > 0.0f ? (t - points[i - 1][0]) / span : 1.0f;
				return points[i - 1][1] + (points[i][1] - points[i - 1][1]) * k;
			}
		}
		return points.back()[1];
	}

	ParticleEffects::Curve ParticleEffects::Curve::read(const nlohmann::json& data, float constant)
	{
		Curve curve;
		if (data.is_number()) curve.points.push_back({ 0.0f, data.get<float>() });
		else if (data.is_array())
		{
			for (const auto& point : data)
				if (point.is_array() && point.size() >= 2 && point[0].is_number() && point[1].is_number())
					curve.points.push_back({ point[0].get<float>(), point[1].get<float>() });
		}
		if (curve.points.empty()) curve.points.push_back({ 0.0f, constant });
		std::sort(curve.points.begin(), curve.points.end(), [](const auto& a, const auto& b) { return a[0] < b[0]; });
		return curve;
	}

	std::array<float, 4> ParticleEffects::Gradient::at(float t) const
	{
		if (stops.empty()) return { 1.0f, 1.0f, 1.0f, 1.0f };
		auto color = [](const std::array<float, 5>& s) { return std::array<float, 4>{ s[1], s[2], s[3], s[4] }; };
		if (t <= stops.front()[0]) return color(stops.front());
		for (size_t i = 1; i < stops.size(); ++i)
		{
			if (t <= stops[i][0])
			{
				const float span = stops[i][0] - stops[i - 1][0];
				const float k = span > 0.0f ? (t - stops[i - 1][0]) / span : 1.0f;
				std::array<float, 4> out;
				for (int c = 0; c < 4; ++c) out[c] = stops[i - 1][c + 1] + (stops[i][c + 1] - stops[i - 1][c + 1]) * k;
				return out;
			}
		}
		return color(stops.back());
	}

	ParticleEffects::Gradient ParticleEffects::Gradient::read(const nlohmann::json& data)
	{
		// [[t, r, g, b, a], ...], or one color [r, g, b] / [r, g, b, a]
		Gradient gradient;
		auto stop = [](float t, const nlohmann::json& c, size_t from)
		{
			std::array<float, 5> s{ t, 1.0f, 1.0f, 1.0f, 1.0f };
			for (size_t i = 0; i < 4 && from + i < c.size(); ++i) if (c[from + i].is_number()) s[i + 1] = c[from + i].get<float>();
			return s;
		};
		if (data.is_array() && !data.empty() && data[0].is_number()) gradient.stops.push_back(stop(0.0f, data, 0));
		else if (data.is_array())
		{
			for (const auto& s : data)
				if (s.is_array() && s.size() >= 4 && s[0].is_number()) gradient.stops.push_back(stop(s[0].get<float>(), s, 1));
		}
		std::sort(gradient.stops.begin(), gradient.stops.end(), [](const auto& a, const auto& b) { return a[0] < b[0]; });
		return gradient;
	}

	ParticleEffects::Range ParticleEffects::Range::read(const nlohmann::json& data, float low, float high)
	{
		if (data.is_number()) return { data.get<float>(), data.get<float>() };
		if (data.is_array() && data.size() >= 2 && data[0].is_number() && data[1].is_number()) return { data[0].get<float>(), data[1].get<float>() };
		return { low, high };
	}

	ParticleEffects::ParticleEffects(Pakal::IParticleRenderer* renderer, uint32_t seed) :
		m_graphics(renderer),
		m_random(seed)
	{
	}

	ParticleEffects::~ParticleEffects() = default;

	bool ParticleEffects::load_sheet(const std::string& file)
	{
		bool found = false;
		const nlohmann::json data = nlohmann::json::parse(ResourceMgr.read_text(file, &found), nullptr, false);
		if (!found || !data.is_object() || !data.contains("texture"))
		{
			LOG_WARNING("[ParticleEffects] the sheet %s does not read", file.c_str());
			return false;
		}
		const std::string dir = file.substr(0, file.find_last_of('/') + 1);
		if (m_graphics) m_graphics->set_sheet(dir + data.value("texture", std::string()), data.value("columns", 1), data.value("rows", 1));
		std::lock_guard<std::mutex> lock(m_filesMutex);
		m_sprites.clear();
		// (a named copy: items() of value()'s temporary would outlive it in the loop)
		const nlohmann::json sprites = data.value("sprites", nlohmann::json::object());
		for (const auto& [name, cell] : sprites.items())
			if (cell.is_number_integer()) m_sprites[name] = cell.get<int>();
		return true;
	}

	void ParticleEffects::prefetch(const std::vector<std::string>& files)
	{
		for (const auto& file : files) effect(file);
	}

	std::shared_ptr<const ParticleEffects::Effect> ParticleEffects::effect(const std::string& file)
	{
		{
			std::lock_guard<std::mutex> lock(m_filesMutex);
			const auto found = m_files.find(file);
			if (found != m_files.end()) return found->second;
		}
		bool found = false;
		const nlohmann::json data = nlohmann::json::parse(ResourceMgr.read_text(file, &found), nullptr, false);
		std::shared_ptr<const Effect> made;
		if (!found || !data.is_object())
		{
			LOG_WARNING("[ParticleEffects] the effect %s does not read", file.c_str());
		}
		else
		{
			made = parse(data, file, 0);
		}
		std::lock_guard<std::mutex> lock(m_filesMutex);
		m_files[file] = made;   // (a file that does not read is not read again)
		return made;
	}

	std::shared_ptr<const ParticleEffects::Effect> ParticleEffects::parse(const nlohmann::json& data, const std::string& file, int depth)
	{
		auto made = std::make_shared<Effect>();
		const std::string dir = file.substr(0, file.find_last_of('/') + 1);
		auto sprite = [this](const nlohmann::json& value)
		{
			if (value.is_number_integer()) return value.get<int>();
			if (value.is_string())
			{
				std::lock_guard<std::mutex> lock(m_filesMutex);
				const auto found = m_sprites.find(value.get<std::string>());
				if (found != m_sprites.end()) return found->second;
				LOG_WARNING("[ParticleEffects] no sprite '%s' in the sheet (load_sheet before the effects)", value.get<std::string>().c_str());
			}
			return 0;
		};
		// another effect: a file (next to this one) or one written in place
		auto child = [this, &dir, &file, depth](const nlohmann::json& value) -> std::shared_ptr<const Effect>
		{
			if (depth >= 3) return nullptr;   // (an effect that starts itself)
			if (value.is_string()) return effect(dir + value.get<std::string>());
			if (value.is_object()) return parse(value.contains("emitters") ? value : nlohmann::json{ { "emitters", nlohmann::json::array({ value }) } }, file, depth + 1);
			return nullptr;
		};
		const nlohmann::json emitters = data.value("emitters", nlohmann::json::array());
		for (const auto& e : emitters)
		{
			if (!e.is_object()) continue;
			Emitter emitter;
			const std::string shape = e.value("shape", std::string("point"));
			emitter.shape = shape == "sphere" ? Emitter::Shape::Sphere : shape == "cone" ? Emitter::Shape::Cone : shape == "box" ? Emitter::Shape::Box : Emitter::Shape::Point;
			emitter.radius = e.value("radius", 0.0f);
			emitter.angle = e.value("angle", 25.0f) * kDegrees;
			emitter.elevation = e.value("elevation", 0.0f) * kDegrees;
			const nlohmann::json box = e.value("box", nlohmann::json::array());
			for (size_t i = 0; i < 3 && i < box.size(); ++i) if (box[i].is_number()) emitter.box[i] = box[i].get<float>();
			emitter.burst = e.value("burst", 0);
			emitter.rate = e.value("rate", 0.0f);
			emitter.duration = e.value("duration", emitter.rate > 0.0f ? 1.0f : 0.0f);
			emitter.delay = e.value("delay", 0.0f);
			emitter.loop = e.value("loop", false);
			emitter.most = std::max(1, e.value("max", 256));
			emitter.life = Range::read(e.value("life", nlohmann::json()), 0.5f, 1.0f);
			emitter.speed = Range::read(e.value("speed", nlohmann::json()), 1.0f, 2.0f);
			emitter.size = Range::read(e.value("size", nlohmann::json()), 0.1f, 0.2f);
			const Range rotation = Range::read(e.value("rotation", nlohmann::json()), 0.0f, 0.0f);
			emitter.rotation = { rotation.low * kDegrees, rotation.high * kDegrees };
			const Range spin = Range::read(e.value("spin", nlohmann::json()), 0.0f, 0.0f);
			emitter.spin = { spin.low * kDegrees, spin.high * kDegrees };
			emitter.startHeight = e.value("height", 0.0f);
			emitter.gravity = e.value("gravity", 0.0f);
			emitter.drag = e.value("drag", 0.0f);
			emitter.sizeOverLife = Curve::read(e.value("sizeOverLife", nlohmann::json()), 1.0f);
			emitter.color = Gradient::read(e.value("color", nlohmann::json()));
			emitter.additive = e.value("blend", std::string("alpha")) == "add";
			emitter.sprite = sprite(e.value("sprite", nlohmann::json()));
			emitter.frames = std::max(1, e.value("frames", 1));
			emitter.fps = e.value("fps", 0.0f);
			const std::string floor = e.value("floor", std::string("none"));
			emitter.floor = floor == "stay" ? Emitter::Floor::Stay : floor == "bounce" ? Emitter::Floor::Bounce : Emitter::Floor::None;
			emitter.restitution = e.value("restitution", emitter.restitution);
			emitter.friction = e.value("friction", emitter.friction);
			const nlohmann::json decal = e.value("decal", nlohmann::json());
			if (decal.is_object())
			{
				emitter.decal.on = true;
				emitter.decal.sprite = sprite(decal.value("sprite", nlohmann::json()));
				emitter.decal.size = Range::read(decal.value("size", nlohmann::json()), 0.2f, 0.4f);
				emitter.decal.seconds = decal.value("seconds", emitter.decal.seconds);
				emitter.decal.fade = decal.value("fade", emitter.decal.fade);
				const Gradient color = Gradient::read(decal.value("color", nlohmann::json()));
				emitter.decal.color = color.at(0.0f);
				emitter.decal.additive = decal.value("blend", std::string("alpha")) == "add";
			}
			const nlohmann::json sub = e.value("subEmitters", nlohmann::json::object());
			if (sub.is_object())
			{
				emitter.onDeath = child(sub.value("death", nlohmann::json()));
				emitter.onLand = child(sub.value("land", nlohmann::json()));
			}
			emitter.gpu = e.value("gpu", std::string("auto")) == "never" ? Emitter::Gpu::Never : Emitter::Gpu::Auto;
			// what the GPU's particles do (GpuParticles): one color whose alpha goes from start to end, a size from start to
			// end, gravity, no drag or turn, landing on a flat floor (staying, or bouncing as its chunks do), no sprite, nothing
			// started from them and no stain
			const bool oneColor = emitter.color.stops.size() <= 1 || (emitter.color.stops.size() == 2 &&
				emitter.color.stops[0][1] == emitter.color.stops[1][1] && emitter.color.stops[0][2] == emitter.color.stops[1][2] &&
				emitter.color.stops[0][3] == emitter.color.stops[1][3] && emitter.color.stops[0][0] == 0.0f && emitter.color.stops[1][0] == 1.0f);
			const bool linearSize = emitter.sizeOverLife.points.size() <= 1 || (emitter.sizeOverLife.points.size() == 2 &&
				emitter.sizeOverLife.points[0][0] == 0.0f && emitter.sizeOverLife.points[1][0] == 1.0f);
			emitter.fitsGpu = emitter.gpu == Emitter::Gpu::Auto && oneColor && linearSize && emitter.drag == 0.0f && emitter.sprite == 0 &&
				emitter.floor != Emitter::Floor::None && !emitter.decal.on && !emitter.onDeath && !emitter.onLand;
			made->length = std::max(made->length, emitter.loop ? 0.0f : emitter.delay + emitter.duration);
			made->emitters.push_back(std::move(emitter));
		}
		return made;
	}

	ParticleEffects::Handle ParticleEffects::play(const std::string& file, const Pakal::Vec2& at, float height, const Pakal::Vec2& direction, float scale)
	{
		const std::shared_ptr<const Effect> made = effect(file);
		if (!made) return 0;
		const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
		const float dx = length > 0.0001f ? direction.x / length : 1.0f, dy = length > 0.0001f ? direction.y / length : 0.0f;
		Running running;
		running.handle = m_next++;
		if (m_next == 0) m_next = 1;
		running.effect = made;
		running.x = at.x; running.y = at.y; running.h = height; running.dx = dx; running.dy = dy; running.scale = scale;
		running.owed.assign(made->emitters.size(), 0.0f);
		running.alive.assign(made->emitters.size(), 0);
		running.burst.assign(made->emitters.size(), false);
		m_running.push_back(std::move(running));
		return m_running.back().handle;
	}

	void ParticleEffects::play_at(const std::shared_ptr<const Effect>& made, float x, float y, float h, float dx, float dy, float scale)
	{
		if (!made) return;
		Running running;
		running.handle = 0;   // (started by a particle: nobody holds it)
		running.effect = made;
		running.x = x; running.y = y; running.h = h; running.dx = dx; running.dy = dy; running.scale = scale;
		running.owed.assign(made->emitters.size(), 0.0f);
		running.alive.assign(made->emitters.size(), 0);
		running.burst.assign(made->emitters.size(), false);
		m_running.push_back(std::move(running));
	}

	void ParticleEffects::stop(Handle handle, bool clear)
	{
		if (handle == 0) return;
		for (auto& running : m_running) if (running.handle == handle) running.stopped = true;
		if (clear) m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(), [handle](const Particle& p) { return p.owner == handle; }), m_particles.end());
	}

	void ParticleEffects::set_position(Handle handle, const Pakal::Vec2& at, float height, const Pakal::Vec2& direction)
	{
		if (handle == 0) return;
		const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
		for (auto& running : m_running)
		{
			if (running.handle != handle) continue;
			running.x = at.x; running.y = at.y; running.h = height;
			if (length > 0.0001f) { running.dx = direction.x / length; running.dy = direction.y / length; }
		}
	}

	bool ParticleEffects::playing(Handle handle) const
	{
		if (handle == 0) return false;
		for (const auto& running : m_running) if (running.handle == handle && !running.stopped) return true;
		for (const auto& particle : m_particles) if (particle.owner == handle) return true;
		return false;
	}

	void ParticleEffects::clear()
	{
		m_running.clear();
		m_particles.clear();
		m_decals.clear();
		if (m_graphics) m_graphics->clear_gpu();
	}

	void ParticleEffects::spawn(Running& running, size_t index, int count)
	{
		const Emitter& emitter = running.effect->emitters[index];
		const float s = running.scale;
		// the play direction on the floor, and across it
		const float fx = running.dx, fy = running.dy, ax = -fy, ay = fx;
		const bool gpu = emitter.fitsGpu && m_graphics && m_graphics->gpu_available();
		for (int i = 0; i < count; ++i)
		{
			if (!gpu && (running.alive[index] >= emitter.most || m_particles.size() >= kMostParticles)) return;
			// where it starts (along, across, up) and where it flies (a unit vector)
			float along = 0.0f, across = 0.0f, up = 0.0f, va = 0.0f, vc = 0.0f, vu = 0.0f;
			auto random_unit = [this](float& x, float& y, float& z)
			{
				const float u = m_random.uniform(-1.0f, 1.0f), t = m_random.uniform(0.0f, 6.2831853f), r = std::sqrt(std::max(0.0f, 1.0f - u * u));
				x = r * std::cos(t); y = r * std::sin(t); z = u;
			};
			switch (emitter.shape)
			{
			case Emitter::Shape::Point:
				random_unit(va, vc, vu);
				break;
			case Emitter::Shape::Sphere:
			{
				random_unit(va, vc, vu);
				const float r = emitter.radius * std::cbrt(m_random.uniform(0.0f, 1.0f));
				along = va * r; across = vc * r; up = vu * r;
				break;
			}
			case Emitter::Shape::Cone:
			{
				// around its axis (the direction raised by 'elevation'), within 'angle'
				const float cosMax = std::cos(emitter.angle);
				const float c = m_random.uniform(cosMax, 1.0f), sn = std::sqrt(std::max(0.0f, 1.0f - c * c));
				const float t = m_random.uniform(0.0f, 6.2831853f);
				const float local[3] = { c, sn * std::cos(t), sn * std::sin(t) };   // axis, across, up (before raising it)
				const float ce = std::cos(emitter.elevation), se = std::sin(emitter.elevation);
				va = local[0] * ce - local[2] * se;
				vc = local[1];
				vu = local[0] * se + local[2] * ce;
				const float r = emitter.radius * std::sqrt(m_random.uniform(0.0f, 1.0f)), rt = m_random.uniform(0.0f, 6.2831853f);
				across = r * std::cos(rt); up = r * std::sin(rt);
				break;
			}
			case Emitter::Shape::Box:
				along = m_random.uniform(-emitter.box[0], emitter.box[0]);
				across = m_random.uniform(-emitter.box[1], emitter.box[1]);
				up = m_random.uniform(-emitter.box[2], emitter.box[2]);
				random_unit(va, vc, vu);
				break;
			}
			const float speed = m_random.uniform(emitter.speed.low, emitter.speed.high) * s;
			Particle p;
			p.x = running.x + (fx * along + ax * across) * s;
			p.y = running.y + (fy * along + ay * across) * s;
			p.h = running.h + (emitter.startHeight + up) * s;
			p.vx = (fx * va + ax * vc) * speed;
			p.vy = (fy * va + ay * vc) * speed;
			p.vh = vu * speed;
			p.age = 0.0f;
			p.life = std::max(0.01f, m_random.uniform(emitter.life.low, emitter.life.high));
			p.size = m_random.uniform(emitter.size.low, emitter.size.high) * s;
			p.rotation = m_random.uniform(emitter.rotation.low, emitter.rotation.high);
			p.spin = m_random.uniform(emitter.spin.low, emitter.spin.high);
			p.emitter = &emitter;
			p.effect = running.effect;
			p.owner = running.handle;
			p.landed = false;
			if (gpu)
			{
				// handed over once, in render space (y up, z = -y); the GPU's floor is flat at 0
				Pakal::IParticleRenderer::GpuParticle g;
				g.position[0] = p.x; g.position[1] = p.h; g.position[2] = -p.y;
				g.velocity[0] = p.vx; g.velocity[1] = p.vh; g.velocity[2] = -p.vy;
				g.life = p.life;
				const std::array<float, 4> c0 = emitter.color.at(0.0f), c1 = emitter.color.at(1.0f);
				g.color[0] = c0[0]; g.color[1] = c0[1]; g.color[2] = c0[2];
				g.alpha0 = c0[3]; g.alpha1 = c1[3];
				g.gravity = emitter.gravity;
				g.radius0 = p.size * emitter.sizeOverLife.at(0.0f);
				g.radius1 = p.size * emitter.sizeOverLife.at(1.0f);
				g.additive = emitter.additive;
				g.bounce = emitter.floor == Emitter::Floor::Bounce;
				m_graphics->emit_gpu(g);
				continue;
			}
			m_particles.push_back(std::move(p));
			++running.alive[index];
		}
	}

	void ParticleEffects::update(float seconds)
	{
		if (m_graphics) m_graphics->advance_gpu(seconds);
		const float dt = std::max(0.0f, seconds);
		// the emitters: their bursts, then their rates
		for (size_t r = 0; r < m_running.size(); ++r)
		{
			Running& running = m_running[r];
			const float before = running.time;
			running.time += dt;
			if (running.stopped) continue;
			for (size_t i = 0; i < running.effect->emitters.size(); ++i)
			{
				const Emitter& emitter = running.effect->emitters[i];
				if (running.time < emitter.delay) continue;
				if (!running.burst[i])
				{
					running.burst[i] = true;
					if (emitter.burst > 0) spawn(m_running[r], i, emitter.burst);
				}
				if (emitter.rate <= 0.0f) continue;
				// the seconds of this frame it ran
				const float start = std::max(before, emitter.delay);
				const float end = emitter.loop ? running.time : std::min(running.time, emitter.delay + emitter.duration);
				if (end <= start) continue;
				m_running[r].owed[i] += (end - start) * emitter.rate;
				const int due = static_cast<int>(m_running[r].owed[i]);
				m_running[r].owed[i] -= static_cast<float>(due);
				if (due > 0) spawn(m_running[r], i, due);
			}
			// (spawn may have grown m_running: refer to it by index)
			if (m_running[r].effect->length > 0.0f && m_running[r].time >= m_running[r].effect->length)
			{
				bool loops = false;
				for (const auto& emitter : m_running[r].effect->emitters) loops = loops || emitter.loop;
				if (!loops) m_running[r].stopped = true;
			}
		}

		// the particles
		struct Started { std::shared_ptr<const Effect> effect; float x, y, h, dx, dy, scale; };
		std::vector<Started> started;
		for (size_t i = 0; i < m_particles.size();)
		{
			Particle& p = m_particles[i];
			const Emitter& emitter = *p.emitter;
			p.age += dt;
			if (p.age >= p.life)
			{
				if (emitter.onDeath) started.push_back({ emitter.onDeath, p.x, p.y, p.h, p.vx, p.vy, 1.0f });
				for (auto& running : m_running)
				{
					if (running.effect != p.effect || running.handle != p.owner) continue;
					for (size_t e = 0; e < running.effect->emitters.size(); ++e)
						if (&running.effect->emitters[e] == p.emitter && running.alive[e] > 0) { --running.alive[e]; break; }
					break;
				}
				p = std::move(m_particles.back());
				m_particles.pop_back();
				continue;
			}
			if (!p.landed || emitter.floor == Emitter::Floor::Bounce)
			{
				const float keep = std::max(0.0f, 1.0f - emitter.drag * dt);
				p.vx *= keep; p.vy *= keep; p.vh *= keep;
				p.vh -= emitter.gravity * dt;
				p.x += p.vx * dt; p.y += p.vy * dt; p.h += p.vh * dt;
			}
			p.rotation += p.spin * dt;
			if (emitter.floor != Emitter::Floor::None)
			{
				const float floor = floor_at(p.x, p.y) + kFloorLift;
				if (p.h <= floor)
				{
					const bool first = !p.landed;
					p.h = floor;
					if (emitter.floor == Emitter::Floor::Bounce && p.vh < -1.0f)
					{
						p.vh = -p.vh * emitter.restitution;
						p.vx *= emitter.friction; p.vy *= emitter.friction;
					}
					else
					{
						p.vx = p.vy = p.vh = 0.0f;
						p.landed = true;
					}
					if (first)
					{
						if (emitter.onLand) started.push_back({ emitter.onLand, p.x, p.y, floor, p.vx, p.vy, 1.0f });
						if (emitter.decal.on && m_decals.size() < kMostDecals)
							m_decals.push_back({ p.x, p.y, floor, m_random.uniform(emitter.decal.size.low, emitter.decal.size.high), m_random.uniform(0.0f, 6.2831853f),
								0.0f, &emitter.decal, p.effect });
						else if (emitter.decal.on && !m_decals.empty())
						{
							// the oldest stain goes
							auto oldest = std::max_element(m_decals.begin(), m_decals.end(), [](const Decal& a, const Decal& b) { return a.age < b.age; });
							*oldest = { p.x, p.y, floor, m_random.uniform(emitter.decal.size.low, emitter.decal.size.high), m_random.uniform(0.0f, 6.2831853f),
								0.0f, &emitter.decal, p.effect };
						}
						p.landed = true;
					}
				}
			}
			draw(p);
			++i;
		}
		for (const auto& s : started) play_at(s.effect, s.x, s.y, s.h, s.dx, s.dy, s.scale);

		// the stains on the floor
		for (size_t i = 0; i < m_decals.size();)
		{
			Decal& d = m_decals[i];
			d.age += dt;
			if (d.age >= d.decal->seconds)
			{
				d = std::move(m_decals.back());
				m_decals.pop_back();
				continue;
			}
			if (m_graphics)
			{
				std::array<float, 4> color = d.decal->color;
				const float left = d.decal->seconds - d.age;
				if (d.decal->fade > 0.0f && left < d.decal->fade) color[3] *= left / d.decal->fade;
				const float at[3] = { d.x, d.h, -d.y };
				m_graphics->draw_floor_spot(at, d.size, abgr(color), d.decal->additive, static_cast<uint16_t>(std::max(0, d.decal->sprite)), d.rotation);
			}
			++i;
		}

		// effects that are done: stopped, and none of their particles left
		m_running.erase(std::remove_if(m_running.begin(), m_running.end(), [](const Running& running)
		{
			if (!running.stopped) return false;
			for (int alive : running.alive) if (alive > 0) return false;
			return true;
		}), m_running.end());
	}

	void ParticleEffects::draw(const Particle& p) const
	{
		if (!m_graphics) return;
		const Emitter& emitter = *p.emitter;
		const float t = std::clamp(p.age / p.life, 0.0f, 1.0f);
		const float radius = p.size * emitter.sizeOverLife.at(t);
		if (radius <= 0.0f) return;
		const uint32_t color = abgr(emitter.color.at(t));
		const float at[3] = { p.x, p.h, -p.y };
		int sprite = 0;
		if (emitter.sprite > 0)
		{
			// the flipbook: its frames at 'fps', or spread over its life
			const int frame = emitter.fps > 0.0f ? static_cast<int>(p.age * emitter.fps) % emitter.frames : std::min(emitter.frames - 1, static_cast<int>(t * emitter.frames));
			sprite = emitter.sprite + frame;
		}
		m_graphics->draw_particle(at, radius, color, emitter.additive, static_cast<uint16_t>(sprite), p.rotation);
	}
}
