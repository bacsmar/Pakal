#pragma once
#include "IManager.h"
#include "IComponentProvider.h"
#include "PakalMath.h"

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