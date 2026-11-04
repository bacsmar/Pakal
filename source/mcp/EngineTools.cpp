///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: EngineTools.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "mcp/EngineTools.h"

#include "BasicTask.h"
#include "Engine.h"
#include "GraphicsSystem.h"
#include "LogMgr.h"
#include "Profiler.h"
#include "bgfx/BgfxGraphicsSystem.h"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

namespace Pakal
{
	namespace
	{
		using json = nlohmann::json;
		using Bgfx = BgfxGraphicsSystem;

		json object_schema(json properties = json::object())
		{
			return { { "type", "object" }, { "properties", std::move(properties) } };
		}

		// Runs on the graphics thread and waits for it (what the renderer holds is changed and read there)
		void on_graphics(Engine& engine, const std::function<void()>& work)
		{
			GraphicsSystem* graphics = engine.graphics_system();
			if (!graphics) throw std::runtime_error("no graphics system");
			auto task = graphics->execute_block(work);
			if (task) task->wait();
		}

		Bgfx& bgfx_of(Engine& engine)
		{
			auto* graphics = dynamic_cast<Bgfx*>(engine.graphics_system());
			if (!graphics) throw std::runtime_error("the graphics system is not bgfx's");
			return *graphics;
		}

		const char* const kQualities[] = { "low", "medium", "high", "ultra", "extreme" };

		Bgfx::Quality quality_named(const std::string& name)
		{
			for (int i = 0; i < 5; ++i)
			{
				if (name == kQualities[i]) return static_cast<Bgfx::Quality>(i);
			}
			throw std::runtime_error("no quality '" + name + "' (low, medium, high, ultra, extreme)");
		}

		// The settings as JSON and back: each field by its name, a field not given left as it is
		template <typename T> void field(json& out, const char* name, const T& value) { out[name] = value; }
		void field3(json& out, const char* name, const float* value) { out[name] = { value[0], value[1], value[2] }; }
		template <typename T> void read(const json& in, const char* name, T& value)
		{
			const auto it = in.find(name);
			if (it != in.end() && !it->is_null()) value = it->get<T>();
		}
		void read3(const json& in, const char* name, float* value)
		{
			const auto it = in.find(name);
			if (it == in.end() || !it->is_array() || it->size() != 3) return;
			for (size_t i = 0; i < 3; ++i) value[i] = (*it)[i].get<float>();
		}

		json preset_json(const Bgfx::QualityPreset& p)
		{
			json out;
			field(out, "shadowMapMin", p.shadowMapMin);
			field(out, "shadowMapMax", p.shadowMapMax);
			field(out, "movingOnlyMap", p.movingOnlyMap);
			field(out, "shadowTaps", p.shadowTaps);
			field(out, "shadowSpacing", p.shadowSpacing);
			field(out, "fullCasters", p.fullCasters);
			field(out, "postQuality", p.postQuality);
			field(out, "antialias", p.antialias);
			field(out, "bloom", p.bloom);
			field(out, "clouds", p.clouds);
			return out;
		}

		void read_preset(const json& in, Bgfx::QualityPreset& p)
		{
			read(in, "shadowMapMin", p.shadowMapMin);
			read(in, "shadowMapMax", p.shadowMapMax);
			read(in, "movingOnlyMap", p.movingOnlyMap);
			read(in, "shadowTaps", p.shadowTaps);
			read(in, "shadowSpacing", p.shadowSpacing);
			read(in, "fullCasters", p.fullCasters);
			read(in, "postQuality", p.postQuality);
			read(in, "antialias", p.antialias);
			read(in, "bloom", p.bloom);
			read(in, "clouds", p.clouds);
		}

		json post_json(const Bgfx::PostSettings& p)
		{
			json out;
			field(out, "enabled", p.enabled);
			field(out, "exposure", p.exposure);
			field(out, "contrast", p.contrast);
			field(out, "saturation", p.saturation);
			field3(out, "gain", p.gain);
			field(out, "vignette", p.vignette);
			field(out, "tiltBlur", p.tiltBlur);
			field(out, "tiltFocus", p.tiltFocus);
			field(out, "tiltBand", p.tiltBand);
			field(out, "aoStrength", p.aoStrength);
			field(out, "aoRadius", p.aoRadius);
			field(out, "aoBias", p.aoBias);
			field(out, "aoRange", p.aoRange);
			field(out, "quality", p.quality);
			field3(out, "fogColor", p.fogColor);
			field(out, "fogDensity", p.fogDensity);
			field(out, "fogStart", p.fogStart);
			field(out, "fogMax", p.fogMax);
			field(out, "fogSky", p.fogSky);
			field(out, "fogHorizon", p.fogHorizon);
			field(out, "bloomStrength", p.bloomStrength);
			field(out, "bloomThreshold", p.bloomThreshold);
			field(out, "bloomKnee", p.bloomKnee);
			return out;
		}

		void read_post(const json& in, Bgfx::PostSettings& p)
		{
			read(in, "enabled", p.enabled);
			read(in, "exposure", p.exposure);
			read(in, "contrast", p.contrast);
			read(in, "saturation", p.saturation);
			read3(in, "gain", p.gain);
			read(in, "vignette", p.vignette);
			read(in, "tiltBlur", p.tiltBlur);
			read(in, "tiltFocus", p.tiltFocus);
			read(in, "tiltBand", p.tiltBand);
			read(in, "aoStrength", p.aoStrength);
			read(in, "aoRadius", p.aoRadius);
			read(in, "aoBias", p.aoBias);
			read(in, "aoRange", p.aoRange);
			read(in, "quality", p.quality);
			read3(in, "fogColor", p.fogColor);
			read(in, "fogDensity", p.fogDensity);
			read(in, "fogStart", p.fogStart);
			read(in, "fogMax", p.fogMax);
			read(in, "fogSky", p.fogSky);
			read(in, "fogHorizon", p.fogHorizon);
			read(in, "bloomStrength", p.bloomStrength);
			read(in, "bloomThreshold", p.bloomThreshold);
			read(in, "bloomKnee", p.bloomKnee);
		}

		json shadows_json(const Bgfx::ShadowSettings& s)
		{
			json out;
			field(out, "enabled", s.enabled);
			field(out, "mapSize", s.mapSize);
			field(out, "extent", s.extent);
			field(out, "fitToCamera", s.fitToCamera);
			field(out, "receiverHeight", s.receiverHeight);
			field(out, "strength", s.strength);
			field(out, "bias", s.bias);
			field(out, "normalOffset", s.normalOffset);
			field(out, "cloudStrength", s.cloudStrength);
			field(out, "cloudSize", s.cloudSize);
			return out;
		}

		void read_shadows(const json& in, Bgfx::ShadowSettings& s)
		{
			read(in, "enabled", s.enabled);
			read(in, "mapSize", s.mapSize);
			read(in, "extent", s.extent);
			read(in, "fitToCamera", s.fitToCamera);
			read(in, "receiverHeight", s.receiverHeight);
			read(in, "strength", s.strength);
			read(in, "bias", s.bias);
			read(in, "normalOffset", s.normalOffset);
			read(in, "cloudStrength", s.cloudStrength);
			read(in, "cloudSize", s.cloudSize);
		}

		json settings_json(Bgfx& g)
		{
			json presets = json::object();
			for (int i = 0; i < 5; ++i) presets[kQualities[i]] = preset_json(g.get_quality_preset(static_cast<Bgfx::Quality>(i)));
			return { { "quality", kQualities[static_cast<int>(g.get_quality())] }, { "presets", presets }, { "post", post_json(g.get_post_process()) },
				{ "shadows", shadows_json(g.get_shadows()) }, { "renderScale", g.get_render_scale() }, { "fsr", g.get_fsr() },
				{ "sharpness", g.get_sharpness() }, { "governor", g.get_governor() }, { "governorStatus", g.governor_status() },
				{ "gpuProfiler", g.get_gpu_profiler() }, { "cameraHeld", g.camera_held() }, { "size", { g.get_width(), g.get_height() } } };
		}
	}

	void add_engine_tools(McpServer& server, Engine& engine)
	{
		Engine* e = &engine;
		server.add_tool("get_status", { "Frame rates of the engine and of the graphics system", object_schema(),
			[e](const json&)
			{
				GraphicsSystem* graphics = e->graphics_system();
				return json{ { "engine_fps", e->get_fps() }, { "graphics_fps", graphics ? graphics->get_fps() : 0u } };
			} });
		server.add_tool("screenshot", { "Saves the next presented frame as a TGA file (a path where the game can write). Returns the path; the file appears on the following frame",
			object_schema({ { "path", { { "type", "string" }, { "description", "Destination .tga file" } } } }),
			[e](const json& arguments)
			{
				const std::string path = arguments.at("path").get<std::string>();
				if (path.size() < 5 || path.substr(path.size() - 4) != ".tga") throw std::runtime_error("the path must end in .tga");
				Bgfx& graphics = bgfx_of(*e);
				on_graphics(*e, [&graphics, path]() { graphics.request_screenshot(path.c_str()); });
				return json{ { "path", path } };
			} });
		server.add_tool("get_stats", { "The last second as the profiler records it: fps, frame_ms (p50, p95, p99, max), sections (ms per frame), "
			"counters (gpu.frame_ms, gpu.view.<pass>_ms, gpu.draws, gpu.primitives, cpu.submit_ms, mem.process_mb, thermal.status, render.*, governor.*), "
			"and its clock (seconds since the game started)", object_schema(),
			[](const json&)
			{
				const std::vector<std::string> records = Profiler::get().records_after(-1.0);
				if (records.empty()) return json{ { "waiting", "the first second is not over yet" } };
				return json::parse(records.back());
			} });
		server.add_tool("get_profile", { "The profiler's records kept in memory (one a second, the last 600) after a clock: what a recording on a computer appends (NDJSON, tools/pakal_prof.py)",
			object_schema({ { "after", { { "type", "number" }, { "description", "The clock of the last record already taken (seconds; -1: all)" } } } }),
			[](const json& arguments)
			{
				json records = json::array();
				for (const auto& text : Profiler::get().records_after(arguments.value("after", -1.0))) records.push_back(json::parse(text));
				return json{ { "records", records } };
			} });
		server.add_tool("record_start", { "Records the profile into a file of the device (NDJSON, a summary at the end) until record_stop",
			object_schema({ { "path", { { "type", "string" } } } }),
			[](const json& arguments)
			{
				const std::string path = arguments.at("path").get<std::string>();
				if (!Profiler::get().start(path)) throw std::runtime_error("cannot write " + path);
				return json{ { "recording", path } };
			} });
		server.add_tool("record_stop", { "Ends the recording record_start began (its summary written)", object_schema(),
			[](const json&)
			{
				const std::string path = Profiler::get().recording_path();
				Profiler::get().stop();
				return json{ { "recorded", path } };
			} });
		server.add_tool("get_settings", { "How the picture is drawn: the quality level and every preset's fields, post-process, shadows, render scale, FSR, sharpness, the governor",
			object_schema(),
			[e](const json&)
			{
				Bgfx& graphics = bgfx_of(*e);
				json out;
				on_graphics(*e, [&]() { out = settings_json(graphics); });
				return out;
			} });
		server.add_tool("set_settings", { "Changes how the picture is drawn, now. Any of: quality (low..extreme), preset {field: value} (of the quality "
			"level given, or the current one), post {field: value}, shadows {field: value}, renderScale (0.3..1), fsr, sharpness, governor (on/off), "
			"gpuProfiler (the GPU's time per pass). Returns the settings after",
			object_schema({ { "quality", { { "type", "string" } } }, { "preset", { { "type", "object" } } }, { "post", { { "type", "object" } } },
				{ "shadows", { { "type", "object" } } }, { "renderScale", { { "type", "number" } } }, { "fsr", { { "type", "boolean" } } },
				{ "sharpness", { { "type", "number" } } }, { "governor", { { "type", "boolean" } } }, { "gpuProfiler", { { "type", "boolean" } } } }),
			[e](const json& arguments)
			{
				Bgfx& graphics = bgfx_of(*e);
				json out;
				std::string error;
				on_graphics(*e, [&]()
				{
					try
					{
						if (arguments.contains("quality")) graphics.set_quality(quality_named(arguments["quality"].get<std::string>()));
						if (arguments.contains("preset"))
						{
							const Bgfx::Quality quality = arguments.contains("quality") ? quality_named(arguments["quality"].get<std::string>()) : graphics.get_quality();
							Bgfx::QualityPreset preset = graphics.get_quality_preset(quality);
							read_preset(arguments["preset"], preset);
							graphics.set_quality_preset(quality, preset);
						}
						if (arguments.contains("post"))
						{
							Bgfx::PostSettings post = graphics.get_post_process();
							read_post(arguments["post"], post);
							graphics.set_post_process(post);
						}
						if (arguments.contains("shadows"))
						{
							Bgfx::ShadowSettings shadows = graphics.get_shadows();
							read_shadows(arguments["shadows"], shadows);
							graphics.set_shadows(shadows);
						}
						if (arguments.contains("renderScale")) graphics.set_render_scale(std::clamp(arguments["renderScale"].get<float>(), 0.3f, 1.0f));
						if (arguments.contains("fsr")) graphics.set_fsr(arguments["fsr"].get<bool>());
						if (arguments.contains("sharpness")) graphics.set_sharpness(arguments["sharpness"].get<float>());
						if (arguments.contains("governor")) graphics.set_governor(arguments["governor"].get<bool>(), 60);
						if (arguments.contains("gpuProfiler")) graphics.set_gpu_profiler(arguments["gpuProfiler"].get<bool>());
						out = settings_json(graphics);
					}
					catch (const std::exception& problem)
					{
						error = problem.what();
					}
				});
				if (!error.empty()) throw std::runtime_error(error);
				return out;
			} });
		server.add_tool("set_camera", { "Holds the camera at a pose (render space: x east, y up, z south; meters), whatever the game does with it, so "
			"settings are compared on the same picture; release: true lets it go",
			object_schema({ { "eye", { { "type", "array" } } }, { "look", { { "type", "array" } } }, { "release", { { "type", "boolean" } } } }),
			[e](const json& arguments)
			{
				Bgfx& graphics = bgfx_of(*e);
				if (arguments.value("release", false))
				{
					on_graphics(*e, [&]() { graphics.release_camera(); });
					return json{ { "held", false } };
				}
				const json eye = arguments.at("eye"), look = arguments.at("look");
				if (!eye.is_array() || eye.size() != 3 || !look.is_array() || look.size() != 3) throw std::runtime_error("eye and look are [x, y, z]");
				const float at[3] = { eye[0].get<float>(), eye[1].get<float>(), eye[2].get<float>() };
				const float to[3] = { look[0].get<float>(), look[1].get<float>(), look[2].get<float>() };
				on_graphics(*e, [&]() { graphics.hold_camera(at, to); });
				return json{ { "held", true }, { "eye", eye }, { "look", look } };
			} });
	}

	std::unique_ptr<McpServer> start_engine_tools(Engine& engine)
	{
		const char* portText = std::getenv("PAKAL_MCP_PORT");
		if (!portText || !*portText) return nullptr;
		auto server = std::make_unique<McpServer>();
		add_engine_tools(*server, engine);
		std::string error;
		const unsigned short port = static_cast<unsigned short>(std::atoi(portText));
		const char* token = std::getenv("PAKAL_MCP_TOKEN");
		if (!server->start(port, token ? token : "", error))
		{
			LOG_ERROR("[EngineTools] MCP server not started: %s", error.c_str());
			return nullptr;
		}
		// measured live from now on (the records the tools read); the renderer then times each pass too
		Profiler::get().set_live(true);
		return server;
	}
}
