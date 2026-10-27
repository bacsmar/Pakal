///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: main.cpp
// Purpose: Shows one 3D model with a directional light and a sky dome. Skinned models play their
//          first two animation clips in turn.
//
// Usage: PakalMeshViewer <model> [seconds=10] [scale=1] [pitchDegrees=0] [screenshot.tga]
//   model          Any model file Assimp reads, e.g. a Collada .dae
//   seconds        How long the window stays open before the application ends
//   scale          Uniform scale applied to the model
//   pitchDegrees   Rotation around X, e.g. -90 for models exported with Z up
//   screenshot     Optional .tga path. Frames are saved during clip 0 and during clip 1 (suffix _clip1)
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Engine.h>
#include <IPakalApplication.h>
#include <OSManager.h>
#include <bgfx/BgfxGraphicsSystem.h>
#include <bgfx/CameraComponent3D_Bgfx.h>
#include <bgfx/MeshComponent3D_Bgfx.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

using namespace Pakal;

namespace
{
	const float kDegToRad = 3.14159265358979f / 180.0f;

	// "shot.tga" + "_clip1" -> "shot_clip1.tga"
	std::string with_suffix(const std::string& path, const std::string& suffix)
	{
		const size_t dot = path.find_last_of('.');
		if (dot == std::string::npos)
		{
			return path + suffix;
		}
		return path.substr(0, dot) + suffix + path.substr(dot);
	}
}

class MeshViewerApplication : public IPakalApplication
{
public:
	MeshViewerApplication(std::string modelPath, float seconds, float scale, float pitchDegrees, std::string screenshotPath) :
		m_modelPath(std::move(modelPath)),
		m_seconds(seconds),
		m_scale(scale),
		m_pitchDegrees(pitchDegrees),
		m_screenshotPath(std::move(screenshotPath))
	{
	}

	const char* get_name() override
	{
		return "MeshViewerApplication";
	}

	void start(Engine& engine) override
	{
		m_graphics = static_cast<BgfxGraphicsSystem*>(engine.graphics_system());
		if (!m_graphics)
		{
			std::cerr << "No graphics system" << std::endl;
			engine.os_manager()->event_app_finished();
			return;
		}

		// bgfx objects must be created on the graphics thread
		auto task = m_graphics->execute_block([this]()
		{
			m_graphics->set_directional_light(0.4f, -1.0f, -0.6f, 1.0f, 0.95f, 0.85f);
			m_graphics->set_ambient_light(0.25f, 0.25f, 0.3f);
			m_graphics->set_sky_colors(0.25f, 0.45f, 0.75f, 0.75f, 0.82f, 0.9f);

			m_mesh = std::make_unique<MeshComponent3D_Bgfx>(m_graphics);
			if (!m_mesh->load_model(m_modelPath))
			{
				std::cerr << "Could not load model: " << m_modelPath << std::endl;
			}
			const float pitch = m_pitchDegrees * kDegToRad;
			m_mesh->set_position(0.0f, 0.0f, 0.0f);
			m_mesh->set_rotation(pitch, 0.0f, 0.0f);
			m_mesh->set_scale(m_scale);

			if (m_mesh->is_skinned() && m_mesh->get_animation_count() > 0)
			{
				const int clipCount = m_mesh->get_animation_count();
				std::cout << "Model has " << clipCount << " animation clips; first ones:" << std::endl;
				for (int i = 0; i < clipCount && i < 4; ++i)
				{
					std::cout << "  " << i << ": " << m_mesh->get_animation_name(i) << std::endl;
				}
				// MESHVIEWER_CLIP=<name> plays that clip for the whole session instead of alternating
				const char* clip = std::getenv("MESHVIEWER_CLIP");
				const int chosen = clip ? m_mesh->find_animation(clip) : -1;
				m_fixedClip = chosen >= 0;
				m_mesh->set_in_place(m_fixedClip);
				m_mesh->play_animation(m_fixedClip ? chosen : 0, true);
			}

			// Frame the model: center and radius after scale and the X rotation
			float bmin[3], bmax[3];
			m_mesh->get_bounds(bmin, bmax);
			const float cx = 0.5f * (bmin[0] + bmax[0]) * m_scale;
			const float cy0 = 0.5f * (bmin[1] + bmax[1]) * m_scale;
			const float cz0 = 0.5f * (bmin[2] + bmax[2]) * m_scale;
			const float cy = cy0 * std::cos(pitch) - cz0 * std::sin(pitch);
			const float cz = cy0 * std::sin(pitch) + cz0 * std::cos(pitch);
			const float radius = m_scale * 0.5f * std::sqrt(
				(bmax[0] - bmin[0]) * (bmax[0] - bmin[0]) +
				(bmax[1] - bmin[1]) * (bmax[1] - bmin[1]) +
				(bmax[2] - bmin[2]) * (bmax[2] - bmin[2]));
			const float safeRadius = radius > 0.0f ? radius : 1.0f;

			// Camera on +Z looking at the center, from slightly above. The 60 degree field of view
			// fits a sphere of this radius at about 2.4 radii.
			const float distance = 2.4f * safeRadius;
			m_camera = std::make_unique<CameraComponent3D_Bgfx>(m_graphics);
			m_camera->set_perspective(60.0f, safeRadius * 0.01f, safeRadius * 20.0f);
			m_camera->set_position(cx, cy + 0.35f * safeRadius, cz + distance);
			m_camera->look_at(cx, cy, cz);
			m_graphics->set_active_camera(m_camera.get());
		});
		if (task)
		{
			task->wait();
		}

		// Timeline of the session (seconds from start). Clip 0 plays first; clip 1 takes over at a
		// third of the time (when the model has it), and clip 0 returns at two thirds.
		const double total = m_seconds;
		const bool canSwitch = m_mesh && m_mesh->is_skinned() && m_mesh->get_animation_count() >= 2 && !m_fixedClip;

		struct Step
		{
			double at;
			std::function<void()> action;
		};
		std::vector<Step> steps;

		steps.push_back({ total / 6.0, [this]()
		{
			if (!m_screenshotPath.empty())
			{
				queue_on_graphics([this]() { m_graphics->request_screenshot(m_screenshotPath.c_str()); });
			}
		} });

		if (canSwitch)
		{
			steps.push_back({ total / 3.0, [this]() { queue_on_graphics([this]() { m_mesh->play_animation(1, true); }); } });
		}

		steps.push_back({ total / 2.0, [this, canSwitch]()
		{
			if (canSwitch && !m_screenshotPath.empty())
			{
				const std::string path = with_suffix(m_screenshotPath, "_clip1");
				queue_on_graphics([this, path]() { m_graphics->request_screenshot(path.c_str()); });
			}
		} });

		if (canSwitch)
		{
			steps.push_back({ 2.0 * total / 3.0, [this]() { queue_on_graphics([this]() { m_mesh->play_animation(0, true); }); } });
		}

		Engine* enginePtr = &engine;
		steps.push_back({ total, [enginePtr]() { enginePtr->os_manager()->event_app_finished(); } });

		m_timer = std::thread([steps]()
		{
			double elapsed = 0.0;
			for (const auto& step : steps)
			{
				const double wait = step.at - elapsed;
				if (wait > 0.0)
				{
					std::this_thread::sleep_for(std::chrono::duration<double>(wait));
				}
				elapsed = step.at;
				step.action();
			}
		});
	}

	void end(Engine& engine) override
	{
		(void)engine;
		if (m_timer.joinable())
		{
			m_timer.join();
		}

		if (!m_graphics)
		{
			return;
		}

		// Release the bgfx objects on the graphics thread, before the graphics system shuts down
		auto task = m_graphics->execute_block([this]()
		{
			m_mesh.reset();
			m_graphics->clear_active_camera(m_camera.get());
			m_camera.reset();
		});
		if (task)
		{
			task->wait();
		}
	}

private:
	// Runs bgfx work on the graphics thread without waiting for it
	void queue_on_graphics(std::function<void()> work)
	{
		m_graphics->execute_block(work);
	}

	std::string m_modelPath;
	float m_seconds;
	float m_scale;
	float m_pitchDegrees;
	std::string m_screenshotPath;
	bool m_fixedClip = false;

	BgfxGraphicsSystem* m_graphics = nullptr;
	std::unique_ptr<CameraComponent3D_Bgfx> m_camera;
	std::unique_ptr<MeshComponent3D_Bgfx> m_mesh;
	std::thread m_timer;
};

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		std::cerr << "Usage: PakalMeshViewer <model> [seconds=10] [scale=1] [pitchDegrees=0] [screenshot.tga]" << std::endl;
		return 1;
	}

	const float seconds = argc > 2 ? static_cast<float>(std::atof(argv[2])) : 10.0f;
	const float scale = argc > 3 ? static_cast<float>(std::atof(argv[3])) : 1.0f;
	const float pitch = argc > 4 ? static_cast<float>(std::atof(argv[4])) : 0.0f;
	const std::string screenshot = argc > 5 ? argv[5] : std::string();

	try
	{
		Engine::Settings settings;
		Engine engine(settings);

		MeshViewerApplication app(argv[1], seconds, scale, pitch, screenshot);
		engine.run(&app);
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
