#pragma once
#include "IManager.h"
#include "IComponentProvider.h"
#include "PakalMath.h"
#include "MusicMixer.h"

#include <string>

namespace Pakal
{

	struct SoundSettings
	{
		float volume = 100;
		float pitch = 1;
		tmath::vector3df position;
		bool relative_to_source = false;

		// Spatialization (mono sounds only: SFML ignores position for stereo buffers)
		float min_distance = 1.0f; // gain is full inside this distance
		float attenuation = 1.0f;  // falloff beyond min_distance; 0 disables it

		//valid only for MusicComponent
		bool loop = false;
	};

	class _PAKALExport SoundManager : public IManager,  public IComponentProvider
	{
	public:
		virtual ~SoundManager() {};

		virtual void set_volume(float volume) = 0;
		virtual float get_volume() = 0;

		// Where the player hears from. Sounds with a position are heard relative to it.
		virtual void set_listener(const tmath::vector3df& position, const tmath::vector3df& forward, const tmath::vector3df& up) {}

		// Music: an OGG Vorbis resource (a folder, a zip, the APK) that crossfades with the one playing (MusicMixer).
		// False when the platform has no music or the file is not one.
		virtual bool play_music(const std::string& path, const MusicSettings& settings) { (void)path; (void)settings; return false; }
		virtual void stop_music(float fadeSeconds) { (void)fadeSeconds; }
		virtual void set_music_volume(float volume) { (void)volume; }
		// Everything that sounds holds where it is (the engine paused: the app went to the background) and goes on from there
		virtual void set_paused(bool paused) { (void)paused; }
	};

	class NullSoundManager: public SoundManager
	{
	public:
		void register_component_factories(std::vector<IComponentFactory*>& factories) override {};
		void initialize() override {};
		void terminate() override {};
		~NullSoundManager() override {};
		void set_volume(float) override {};
		float get_volume() override { return 0.f; };
	};
}