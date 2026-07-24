#pragma once
#include "Component.h"
#include <string>

namespace Pakal
{
	class _PAKALExport SkeletalAnimationComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(SkeletalAnimationComponent, Component);

	public:
		SkeletalAnimationComponent() = default;
		virtual ~SkeletalAnimationComponent() = default;

		virtual void set_skeleton_asset(const std::string& assetPath) = 0;
		virtual const std::string& get_skeleton_asset() const = 0;

		virtual void set_animation(const std::string& animationName) = 0;
		virtual const std::string& get_animation() const = 0;

		virtual void set_skin(const std::string& skinName) = 0;
		virtual const std::string& get_skin() const = 0;

		virtual void set_looping(bool loop) = 0;
		virtual bool is_looping() const = 0;

		virtual void set_playback_speed(float speed) = 0;
		virtual float get_playback_speed() const = 0;

		virtual void set_time(float seconds) = 0;
		virtual float get_time() const = 0;

		virtual void set_render_scale(float scale) = 0;
		virtual float get_render_scale() const = 0;

		virtual void update(float deltaTime) = 0;
	};
}
