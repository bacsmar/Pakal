#pragma once
#include "Component.h"
#include "BasicTask.h"
#include "math/tm.h"

namespace Pakal
{
	class SpriteComponent : public Component
	{
	public:
		DECLARE_RTTI_WITH_BASE(SpriteComponent, Component);
		struct Settings
		{
			std::string resource_file;
			std::string initial_animation;
			unsigned frame_time_ms;
			bool init_paused;
			bool init_looped;
			tmath::vector3df initial_position;
			Settings() : frame_time_ms(200), init_paused(false), init_looped(true){}
		};

		virtual BasicTaskPtr initialize(const Settings& settings) = 0;
		virtual BasicTaskPtr destroy() = 0;
	};
}