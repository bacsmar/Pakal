#pragma once
#include "Components/UIComponent.h"
#include "RocketGUI.h"
#include <Rocket/Core/Event.h>
#include <Rocket/Core/EventListener.h>

namespace Pakal
{
	class _PAKALExport RocketLabel : public UILabel, public Rocket::Core::EventListener
	{
		DECLARE_RTTI_WITH_BASE(RocketLabel, UILabel);
	public:
		explicit RocketLabel(RocketUI* manager);
		~RocketLabel();

		// from rocket
		void ProcessEvent(Rocket::Core::Event& event) override;

		void enable_ui_events(bool enable) override;

		void set_ui_control(unsigned int documentId, const std::string& controlName) override;
		void set_text(const std::string& text) override;
		void set_visible(bool visible) override;
		void set_color(unsigned color) const override;
		void set_font_color(unsigned color) const override;
		void set_width(unsigned width) const override;
		void set_width_percent(unsigned width) const override;
		int get_width() const override;
		void terminate() override;
	private:

		void suscribe_to_events();
		void unsuscribe_to_events();

		RocketUI* m_rocket_ui = nullptr;
		Rocket::Core::Element* m_element = nullptr;
		unsigned m_document_id = 0;
		std::string m_control_name;
	};
}
