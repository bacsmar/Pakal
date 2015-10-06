#pragma once
#include "IManager.h"
#include "IComponentProvider.h"
#include "math/tm.h"

namespace Pakal
{

	struct SoundSettings
	{
		float volume = 100;
		float pitch = 1;
		tmath::vector3df position;
		bool relative_to_source = false;
		
		//valid only for MusicComponent
		bool loop = false;
	};

	class _PAKALExport SoundManager : public IManager,  public IComponentProvider
	{
	public:
		virtual ~SoundManager() {};

		virtual void set_volume(float volume) = 0;
		virtual float get_volume() = 0;
	};

	class NullSoundManager: public SoundManager
	{
	public:
		void register_component_factories(std::vector<IComponentFactory*>& factories) override {};
		void initialize() override {};
		void terminate() override {};
		~NullSoundManager() override {};
		void set_volume(float volume) override {};
		float get_volume() override { return 0.f; };
	};
}