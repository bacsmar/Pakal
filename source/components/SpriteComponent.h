#pragma once
#include "Component.h"
#include "BasicTask.h"
#include "PakalMath.h"
#include "Sprite.h"

#include "Event.h"

namespace Pakal
{
	class _PAKALExport SpriteComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(SpriteComponent, Component);
	public:
		struct Settings
		{
			SpriteSheetPtr sprite_sheet = std::make_shared<SpriteSheet>();
			tmath::vector3df position;
			bool init_paused = false;
			float speed = 1;
			float size = 1;
			bool is_batched = false;
		};

		Event<void> event_animation_ended;

		virtual BasicTaskPtr initialize(const Settings& settings) = 0;
		virtual BasicTaskPtr terminate() = 0;

		virtual void set_visible(bool visible) = 0;
		virtual bool is_visible() = 0;

		virtual void set_animation(const std::string& name) = 0;		
		virtual void set_animation(unsigned index) = 0;		
		virtual unsigned get_animation_count() const = 0;
		
		virtual void set_flipped(bool val) = 0;
		virtual void set_looped(bool val) = 0;

		virtual bool is_looped() const = 0;
		virtual bool is_flipped() const = 0;

		virtual void set_rotation(float degrees) = 0;
		virtual float get_rotation() const = 0;

		virtual unsigned get_duration() const = 0;

		virtual void set_size(float size) = 0;
			
		virtual tmath::vector3df get_scale() const = 0;
		virtual float get_normalization_factor() const = 0;
		virtual void set_scale(const tmath::vector3df& factor) = 0;

		virtual void set_position(const tmath::vector3df & position) = 0;
		virtual tmath::vector3df  get_position() const = 0;

		virtual void play() = 0;
		virtual void pause() = 0;
		virtual void stop() = 0;
		virtual bool is_playing() const = 0;	
		virtual ~SpriteComponent(){}

	};
}