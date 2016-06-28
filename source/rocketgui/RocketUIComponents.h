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

	class _PAKALExport RocketLayoutElement : public UILayoutElement, public Rocket::Core::EventListener
	{
		
		DECLARE_RTTI_WITH_BASE(RocketLayoutElement, UILayoutElement);
	public:
		explicit RocketLayoutElement(RocketUI* manager);
		~RocketLayoutElement();
		void ProcessEvent(Rocket::Core::Event& event) override;
		void enable_ui_events(bool enable) override;
		void set_visible(bool visible) override;
		void set_ui_control(unsigned int documentId, const std::string& controlName) override;

		bool add_element_inside(UILayoutElement*) const override;
		void remove_inside_elements() const override;
		bool add_element_after(UILayoutElement*) const override;
		void load_style(const std::string& theme_name) const override;
	   

		void set_color(unsigned color) const override;
		void set_font_color(unsigned color) const override;
		void set_width(int width) const override;
		void set_with_percentage(float width) const override;
		void set_heigth(int pixels)const override;
		void set_heigth_percentage(float container_percentage) const override;
		void set_padding(int pixels)const override;
		void set_padding_percentage(float containter_percentage)const override;		

		void clone_on(UILayoutElement* target) const override;

		void set_id_value(const std::string & newId) override;
		std::string get_controled_element_name() const override ;

		 void terminate() override;

		private :
			void suscribe_to_events();
			void unsuscribe_to_events();
			Rocket::Core::Element* get_element() const;
			void set_rocket_element(Rocket::Core::Element* element, bool cloned = false);
			void set_rocket_ui(RocketUI* rocketUi);
			void set_document_id(unsigned documentId);
			void set_control_name(const std::string& controlName);
			unsigned document_id() const;

			RocketUI* m_rocket_ui = nullptr;
			Rocket::Core::Element* m_element = nullptr;
			unsigned m_document_id = 0;
		
			std::string m_control_name;
			bool m_cloned = false;
			bool already_subscribed_to_events = false;

	};
}
