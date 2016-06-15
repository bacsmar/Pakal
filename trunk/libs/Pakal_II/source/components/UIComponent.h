#pragma once
#include "Component.h"
#include "Event.h"

namespace Pakal
{
	class _PAKALExport UIElement
	{
	public:
		virtual ~UIElement(){}			
		virtual void set_ui_control(unsigned int documentId, const std::string& controlName) = 0;
		virtual void set_visible(bool visible) = 0;

		virtual void enable_ui_events(bool enable) = 0;

		struct UIMessage
		{
			const UIElement& sender;
			std::string msg;
		};

		Pakal::Event<const UIMessage&> evt_ui;
	};

	class _PAKALExport UILabel : public Component, public UIElement
	{
		DECLARE_RTTI_WITH_BASE(UILabel, Component);
	public:				
		virtual void set_text(const std::string& text) = 0;
		virtual void set_color(unsigned color) const = 0;
		virtual void set_font_color(unsigned color) const = 0;
		virtual void set_width(unsigned width) const = 0;
		virtual void set_width_percent(unsigned width) const = 0;
		virtual int get_width() const = 0;
		virtual void terminate() = 0;
		virtual ~UILabel(){};
	};

	class _PAKALExport UILayout : public Component, public UIElement
	{
		DECLARE_RTTI_WITH_BASE(UILayout, Component);
	public:
		// pure virtual methods
	};

	class _PAKALExport UILayoutElement : public Component, public UIElement
	{
		DECLARE_RTTI_WITH_BASE(UILayoutElement, Component);
	public:
		// pure virtual methods
	};
}