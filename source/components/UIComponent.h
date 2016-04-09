#pragma once
#include "Component.h"

namespace Pakal
{
	class _PAKALExport UILabel : public Component
	{
		DECLARE_RTTI_WITH_BASE(UILabel, Component);
	public:		
		virtual void set_text(const std::string& text) = 0;
		virtual void set_ui_control(unsigned int documentId, const std::string& controlName) = 0;
		virtual void set_visible(bool visible) = 0;
		virtual void set_color(unsigned color) const = 0;
		virtual void set_font_color(unsigned color) const = 0;
		virtual void set_width(unsigned width) const = 0;
		virtual ~UILabel(){};
	};
}