///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: main.cpp
// Purpose: Plays a tone from four positions around a listener at the origin, looking down -Z.
//          The printed pan and gain are what the engine expects; listen to check that the tone
//          comes from left, right, front and behind in that order.
//
// Usage: PakalAudioDemo [toneHz=440] [distance=10]
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Engine.h>
#include <IPakalApplication.h>
#include <OSManager.h>
#include <SoundManager.h>
#include <SpatialAudio.h>
#include <sfml/SoundManagerSFML.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <thread>

using namespace Pakal;

namespace
{
	struct Spot
	{
		const char* name;
		tmath::vector3df position;
	};
}

class AudioDemoApplication : public IPakalApplication
{
public:
	AudioDemoApplication(float toneHz, float distance) : m_toneHz(toneHz), m_distance(distance) {}

	const char* get_name() override
	{
		return "AudioDemoApplication";
	}

	void start(Engine& engine) override
	{
		m_thread = std::thread([this, &engine]()
		{
			auto* sounds = dynamic_cast<SoundManagerSFML*>(engine.sound_manager());
			if (!sounds)
			{
				std::cerr << "The SFML sound manager is not active" << std::endl;
				engine.os_manager()->event_app_finished();
				return;
			}

			// Listener at the origin, looking down -Z, Y up
			const tmath::vector3df listenerPosition(0.0f, 0.0f, 0.0f);
			const tmath::vector3df forward(0.0f, 0.0f, -1.0f);
			const tmath::vector3df up(0.0f, 1.0f, 0.0f);
			sounds->set_listener(listenerPosition, forward, up);

			auto tone = sounds->create_tone(m_toneHz, 1.0f);
			if (!tone)
			{
				engine.os_manager()->event_app_finished();
				return;
			}

			const float d = m_distance;
			const Spot spots[] = {
				{ "left",   tmath::vector3df(-d, 0.0f, 0.0f) },
				{ "right",  tmath::vector3df(d, 0.0f, 0.0f) },
				{ "front",  tmath::vector3df(0.0f, 0.0f, -d) },
				{ "behind", tmath::vector3df(0.0f, 0.0f, d) },
			};

			for (const Spot& spot : spots)
			{
				const float pan = SpatialAudio::pan_for_listener(listenerPosition, forward, up, spot.position);
				const float distanceToSpot = std::sqrt(spot.position.x * spot.position.x
					+ spot.position.y * spot.position.y + spot.position.z * spot.position.z);
				const float gain = SpatialAudio::inverse_distance_gain(distanceToSpot, 1.0f, 1.0f);
				std::cout << spot.name << ": expected pan " << pan << ", gain " << gain << std::endl;

				SoundSettings settings;
				settings.position = spot.position;
				settings.relative_to_source = false;
				sounds->play_sfx(tone, settings);
				std::this_thread::sleep_for(std::chrono::milliseconds(1200));
			}

			engine.os_manager()->event_app_finished();
		});
	}

	void end(Engine& engine) override
	{
		if (m_thread.joinable())
		{
			m_thread.join();
		}
	}

private:
	float m_toneHz;
	float m_distance;
	std::thread m_thread;
};

int main(int argc, char* argv[])
{
	const float toneHz = argc > 1 ? static_cast<float>(std::atof(argv[1])) : 440.0f;
	const float distance = argc > 2 ? static_cast<float>(std::atof(argv[2])) : 10.0f;

	try
	{
		Engine::Settings settings;
		Engine engine(settings);

		AudioDemoApplication app(toneHz, distance);
		engine.run(&app);
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
