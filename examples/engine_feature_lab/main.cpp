///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: main.cpp
// Purpose: Feature lab for Pakal camera, sprite transform, and audio subsystem diagnostics
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include <BaseGameState.h>
#include <ComponentManager.h>
#include <Engine.h>
#include <EntityManager.h>
#include <GameStateManager.h>
#include <GenericEntity.h>
#include <IPakalApplication.h>
#include <OSManager.h>
#include <components/CameraComponent2D.h>
#include <components/MusicComponent.h>
#include <components/SFXComponent.h>
#include <components/SpriteComponent2D.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace Pakal;

namespace
{
	constexpr float Pi = 3.14159265358979323846f;
	constexpr float LabDurationSeconds = 20.0f;

	void append_text(std::string& output, const char* text)
	{
		while (*text)
		{
			output.push_back(*text++);
		}
	}

	void append_u16(std::string& output, uint16_t value)
	{
		output.push_back(static_cast<char>(value & 0xFF));
		output.push_back(static_cast<char>((value >> 8) & 0xFF));
	}

	void append_u32(std::string& output, uint32_t value)
	{
		output.push_back(static_cast<char>(value & 0xFF));
		output.push_back(static_cast<char>((value >> 8) & 0xFF));
		output.push_back(static_cast<char>((value >> 16) & 0xFF));
		output.push_back(static_cast<char>((value >> 24) & 0xFF));
	}

	std::string create_wave_data(float frequencyHz, float durationSeconds, float gain)
	{
		constexpr uint32_t sampleRate = 44100;
		constexpr uint16_t channels = 1;
		constexpr uint16_t bitsPerSample = 16;
		const uint32_t sampleCount = static_cast<uint32_t>(sampleRate * durationSeconds);
		const uint32_t dataSize = sampleCount * channels * (bitsPerSample / 8);

		std::string data;
		data.reserve(44 + dataSize);
		append_text(data, "RIFF");
		append_u32(data, 36 + dataSize);
		append_text(data, "WAVE");
		append_text(data, "fmt ");
		append_u32(data, 16);
		append_u16(data, 1);
		append_u16(data, channels);
		append_u32(data, sampleRate);
		append_u32(data, sampleRate * channels * (bitsPerSample / 8));
		append_u16(data, channels * (bitsPerSample / 8));
		append_u16(data, bitsPerSample);
		append_text(data, "data");
		append_u32(data, dataSize);

		for (uint32_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
		{
			const float timeSeconds = static_cast<float>(sampleIndex) / static_cast<float>(sampleRate);
			const float envelopeIn = std::min(1.0f, timeSeconds / 0.02f);
			const float envelopeOut = std::min(1.0f, (durationSeconds - timeSeconds) / 0.05f);
			const float envelope = std::min(envelopeIn, envelopeOut);
			const float wave = std::sin(2.0f * Pi * frequencyHz * timeSeconds);
			const auto sample = static_cast<int16_t>(wave * envelope * gain * 32767.0f);
			append_u16(data, static_cast<uint16_t>(sample));
		}

		return data;
	}

	SharedPtr<std::istream> make_audio_stream(const std::string& wavData)
	{
		return std::make_shared<std::istringstream>(wavData, std::ios::binary);
	}

	class FeatureLabState final : public BaseGameState
	{
	public:
		FeatureLabState() : BaseGameState("FeatureLab") {}

	private:
		struct AnimatedSprite
		{
			SpriteComponent2D* sprite = nullptr;
			float positionX = 0.0f;
			float positionY = 0.0f;
			float scaleX = 1.0f;
			float scaleY = 1.0f;
			float rotationSpeed = 0.0f;
			float pulseAmount = 0.0f;
			float pulseSpeed = 1.0f;
		};

		void on_initialize(Engine* engine) override
		{
			m_engine = engine;
			std::cout << "[FeatureLab] Creating camera, sprites, and generated audio" << std::endl;
			create_camera();
			create_sprite_scene();
			create_audio_probe();
		}

		void on_terminate(Engine* engine) override
		{
			if (engine && engine->entity_manager())
			{
				for (const auto& handle : m_entities)
				{
					engine->entity_manager()->request_dispose(handle);
				}
				engine->entity_manager()->process_pending_disposals();
			}

			m_entities.clear();
			m_sprites.clear();
			m_camera = nullptr;
			m_sfx = nullptr;
			m_music = nullptr;
			std::cout << "[FeatureLab] Terminated" << std::endl;
		}

		void on_update(unsigned long dtMilliseconds) override
		{
			const float deltaTime = static_cast<float>(dtMilliseconds) / 1000.0f;
			m_elapsedSeconds += deltaTime;

			update_camera(deltaTime);
			update_sprites();
			update_audio_probe();

			if (m_elapsedSeconds >= LabDurationSeconds && m_engine)
			{
				m_engine->os_manager()->event_app_finished();
			}
		}

		void on_activate(Engine*) override {}
		void on_deactivate(Engine*) override {}

		GenericEntity* create_entity(const std::string& descriptor)
		{
			auto* entityManager = m_engine ? m_engine->entity_manager() : nullptr;
			if (!entityManager)
			{
				return nullptr;
			}

			auto* entity = static_cast<GenericEntity*>(entityManager->create_entity("Pakal::GenericEntity", descriptor));
			if (entity)
			{
				m_entities.push_back(entity->get_handle());
			}
			return entity;
		}

		void create_camera()
		{
			auto* cameraEntity = create_entity("feature_lab_camera");
			if (!cameraEntity)
			{
				std::cerr << "[FeatureLab] Failed to create camera entity" << std::endl;
				return;
			}

			m_camera = cameraEntity->create_component<CameraComponent2D>();
			if (!m_camera)
			{
				std::cerr << "[FeatureLab] Failed to create CameraComponent2D" << std::endl;
				return;
			}

			m_camera->set_viewport(0, 0, 1280, 720);
			m_camera->set_orthographic(32.0f, 18.0f, 0.1f, 100.0f);
			m_camera->set_position(0.0f, 0.0f);
			m_camera->set_zoom(1.0f);
		}

		void create_sprite_scene()
		{
			create_demo_sprite("background", 0x20242DFF, 0.0f, 0.0f, 32.0f, 18.0f, -10, 0.0f, 0.0f, 1.0f);
			create_demo_sprite("axis_x", 0x475569FF, 0.0f, 0.0f, 30.0f, 0.06f, -5, 0.0f, 0.0f, 1.0f);
			create_demo_sprite("axis_y", 0x475569FF, 0.0f, 0.0f, 0.06f, 16.0f, -5, 0.0f, 0.0f, 1.0f);
			create_demo_sprite("red_rotating_square", 0xEF4444FF, -8.0f, -2.0f, 2.4f, 2.4f, 1, 1.35f, 0.15f, 2.0f);
			create_demo_sprite("teal_pulsing_rect", 0x14B8A6FF, 0.0f, 0.0f, 4.0f, 1.4f, 2, -0.75f, 0.25f, 2.6f);
			create_demo_sprite("amber_tall_rect", 0xF59E0BFF, 7.5f, 2.2f, 1.3f, 4.2f, 3, 0.45f, 0.08f, 1.4f);
			create_demo_sprite("blue_scale_probe", 0x60A5FAFF, 0.0f, -5.0f, 1.2f, 1.2f, 4, 2.0f, 0.45f, 4.0f);
		}

		void create_demo_sprite(
			const std::string& descriptor,
			uint32_t color,
			float positionX,
			float positionY,
			float scaleX,
			float scaleY,
			int layer,
			float rotationSpeed,
			float pulseAmount,
			float pulseSpeed)
		{
			auto* entity = create_entity(descriptor);
			if (!entity)
			{
				return;
			}

			auto* sprite = entity->create_component<SpriteComponent2D>();
			if (!sprite)
			{
				std::cerr << "[FeatureLab] Failed to create SpriteComponent2D for " << descriptor << std::endl;
				return;
			}

			sprite->create_solid_color(color, 8, 8);
			sprite->set_position(positionX, positionY);
			sprite->set_scale(scaleX, scaleY);
			sprite->set_layer(layer);
			sprite->set_visible(true);

			m_sprites.push_back({ sprite, positionX, positionY, scaleX, scaleY, rotationSpeed, pulseAmount, pulseSpeed });
		}

		void create_audio_probe()
		{
			auto* audioEntity = create_entity("feature_lab_audio_probe");
			if (!audioEntity)
			{
				return;
			}

			m_sfx = audioEntity->create_component<SFXComponent>();
			if (m_sfx)
			{
				m_sfx->initialize();
				m_sfxReady = m_sfx->add(1, make_audio_stream(create_wave_data(880.0f, 0.16f, 0.25f)), Path("feature_lab_ping.wav"));
				std::cout << "[FeatureLab] SFX probe " << (m_sfxReady ? "ready" : "failed") << std::endl;
			}

			m_music = audioEntity->create_component<MusicComponent>();
			if (m_music)
			{
				SoundSettings settings;
				settings.volume = 12.0f;
				settings.loop = true;

				m_musicReady = m_music->initialize(make_audio_stream(create_wave_data(220.0f, 1.0f, 0.08f)), settings);
				if (m_musicReady)
				{
					m_music->play();
				}
				std::cout << "[FeatureLab] Music probe " << (m_musicReady ? "ready" : "failed") << std::endl;
			}
		}

		void update_camera(float deltaTime)
		{
			if (!m_camera)
			{
				return;
			}

			const float cameraX = std::sin(m_elapsedSeconds * 0.45f) * 1.2f;
			const float cameraY = std::sin(m_elapsedSeconds * 0.30f) * 0.45f;
			m_camera->set_position(cameraX, cameraY);
			m_camera->update(deltaTime);
		}

		void update_sprites()
		{
			for (auto& entry : m_sprites)
			{
				if (!entry.sprite)
				{
					continue;
				}

				const float pulse = 1.0f + std::sin(m_elapsedSeconds * entry.pulseSpeed) * entry.pulseAmount;
				entry.sprite->set_position(entry.positionX, entry.positionY);
				entry.sprite->set_rotation(m_elapsedSeconds * entry.rotationSpeed);
				entry.sprite->set_scale(entry.scaleX * pulse, entry.scaleY * pulse);
			}
		}

		void update_audio_probe()
		{
			if (!m_sfx || !m_sfxReady || m_elapsedSeconds < m_nextSfxTime)
			{
				return;
			}

			SoundSettings settings;
			settings.volume = 35.0f;
			settings.pitch = 0.85f + 0.30f * std::sin(m_elapsedSeconds);
			settings.relative_to_source = true;
			m_sfx->play(1, settings);
			m_nextSfxTime = m_elapsedSeconds + 2.0f;
		}

		Engine* m_engine = nullptr;
		CameraComponent2D* m_camera = nullptr;
		SFXComponent* m_sfx = nullptr;
		MusicComponent* m_music = nullptr;
		bool m_sfxReady = false;
		bool m_musicReady = false;
		float m_elapsedSeconds = 0.0f;
		float m_nextSfxTime = 0.5f;
		std::vector<EntityHandle> m_entities;
		std::vector<AnimatedSprite> m_sprites;
	};
}

class FeatureLabApplication : public IPakalApplication
{
public:
	const char* get_name() override
	{
		return "FeatureLabApplication";
	}

	void start(Engine& engine) override
	{
		std::cout << "Pakal Engine - Feature Lab" << std::endl;
		engine.game_state_manager()->push_state(new FeatureLabState(), true);
	}

	void end(Engine&) override
	{
		std::cout << "Feature lab ended" << std::endl;
	}
};

int main(int, char**)
{
	try
	{
		Engine::Settings settings;
		settings.max_fps = 120;
		settings.graphic_system_settings.max_fps = 120;
		settings.physic_system_settings.max_fps = 120;

		Engine engine(settings);
		FeatureLabApplication app;
		engine.run(&app);
	}
	catch (const std::exception& exception)
	{
		std::cerr << "Error: " << exception.what() << std::endl;
		return 1;
	}

	return 0;
}