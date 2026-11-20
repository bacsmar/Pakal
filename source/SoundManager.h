#pragma once
#include "IManager.h"
#include "IComponentProvider.h"
#include "PakalMath.h"
#include "MusicMixer.h"

#include <memory>
#include <string>

namespace Pakal
{
	// A sound effect a SoundManager loaded (its samples, decoded once): what it holds is the platform's. It is played by
	// the manager that loaded it.
	class SoundEffect
	{
	public:
		virtual ~SoundEffect() = default;
	};

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
		// Sound effects: a resource (a folder, a zip, the APK; WAV, or OGG where the platform decodes it), read and decoded
		// once. nullptr when the platform has no sound or the file does not load.
		virtual std::shared_ptr<SoundEffect> load_effect(const std::string& path) { (void)path; return nullptr; }
		// Plays it now (several play at once): settings.volume 0..100 and pitch; its position, where the platform places
		// sounds (relative_to_source with attenuation 0: heard as is). 'effect' comes from this manager's load_effect.
		virtual void play_effect(const std::shared_ptr<SoundEffect>& effect, const SoundSettings& settings) { (void)effect; (void)settings; }
		// A mono sine tone made in memory (no asset: a test, an audio check), played like a loaded effect. nullptr where the
		// platform has no sound.
		virtual std::shared_ptr<SoundEffect> create_tone(float frequencyHz, float seconds) { (void)frequencyHz; (void)seconds; return nullptr; }
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