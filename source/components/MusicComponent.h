#pragma once

#include "Config.h"
#include "Component.h"
#include "PakalPath.h"
#include "SoundManager.h"
#include "TaskFwd.h"
#include "PakalTime.h"

namespace Pakal
{
	enum class MusicState
	{
		Playing,
		Paused,
		Stopped,
		Invalid
	};

	class _PAKALExport MusicComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(MusicComponent, Component);
	public:
		virtual bool initialize(const Path& resourcePath, const SoundSettings& settings = SoundSettings()) = 0;
		virtual bool initialize(SharedPtr<std::istream> resourceStream, const SoundSettings& settings = SoundSettings()) = 0;
		virtual void terminate() = 0;

		virtual void play() = 0;
		virtual void pause() = 0;
		virtual void stop() = 0;

		virtual void set_volume(float volume) = 0;
		virtual float get_volume() = 0;

		virtual void set_pitch(float pitch) = 0;
		virtual float get_pitch() = 0;

		virtual void set_position(const tmath::vector3df& position) = 0;
		virtual tmath::vector3df get_position() = 0;

		virtual void set_loop(bool loop) = 0;
		virtual bool get_loop() = 0;

		virtual void set_relative_to_listener(bool relative) = 0;
		virtual bool get_relative_to_listener() = 0;


		virtual Time get_duration() = 0;
		virtual unsigned get_channel_count() = 0;
		virtual unsigned get_sample_rate() = 0;
		virtual MusicState get_state() = 0;
	};
}
