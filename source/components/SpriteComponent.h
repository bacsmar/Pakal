#pragma once
#include "Component.h"
#include "BasicTask.h"
#include "math/tm.h"

#include "Event.h"

namespace Pakal
{
	class SpriteComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(SpriteComponent, Component);
	public:
		struct Settings
		{
			std::string resource_file;
			std::string initial_animation;
			float frame_time_factor = 1.f;
			bool init_paused = false;			
			tmath::vector3df initial_position;
		};

		Event<SpriteComponent, void> event_animation_ended;

		virtual BasicTaskPtr initialize(const Settings& settings) = 0;
		virtual BasicTaskPtr terminate() = 0;
		
		virtual void set_animation(const std::string& animationName) = 0;
		// is the current animation looped?
		virtual bool is_looped() const = 0;
		virtual bool get_flipped() const = 0;
		virtual void set_flipped(bool val) = 0;

		virtual void play() = 0;
		virtual void pause() = 0;
		virtual void stop() = 0;
		virtual bool is_playing() const = 0;
	protected:
		inline void fire_event_animation_ended()
		{
			event_animation_ended.notify();
		}
	};
}