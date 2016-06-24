#pragma once
#include "Component.h"
#include "Event.h"

namespace Pakal
{
	class UILayoutElement;

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


	class _PAKALExport UILayoutElement : public Component, public UIElement
	{
		DECLARE_RTTI_WITH_BASE(UILayoutElement, Component);
	public:
		virtual bool add_element_inside(UILayoutElement*) const=0;
		virtual void remove_inside_elements() const = 0;
		virtual bool add_element_after(UILayoutElement*) const = 0;
		virtual void load_style(const std::string& theme_name) const=0;
	

		virtual void set_width(int pixels) const = 0;
		virtual void set_with_percentage(float container_percentage) const = 0;
		virtual void set_heigth(int pixels) const = 0;
		virtual void set_heigth_percentage(float container_percentage) const = 0;
		virtual void set_padding(int pixels) const = 0;
		virtual void set_padding_percentage(float containter_percentage) const = 0;
		virtual void set_alpha_color(unsigned color, unsigned alpha_percentage) const = 0;
		virtual void set_color(unsigned color) const = 0;
		virtual void set_font_color(unsigned color) const = 0;
		
		virtual void set_id_value(const std::string & newId) = 0;
		virtual std::string get_controled_element_name() const=0;
		virtual void clone_on (UILayoutElement* target) const = 0;
		virtual ~UILayoutElement() {};
		virtual void terminate() = 0;
	private:

		// pure virtual methods
	};



}