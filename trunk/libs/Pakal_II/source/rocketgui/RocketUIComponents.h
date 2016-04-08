#pragma once
#include "Components/UIComponent.h"
#include "RocketGUI.h"

namespace Pakal
{
	class _PAKALExport RocketLabel : public UILabel
	{
		DECLARE_RTTI_WITH_BASE(RocketLabel, UILabel);
	public:
		explicit RocketLabel(RocketUI* manager);

		void set_ui_control(unsigned int documentId, const std::string& controlName) override;
		void set_text(const std::string& text) override;
		void set_visible(bool visible) override;
	private:
		RocketUI* m_rocket_ui = nullptr;
		Rocket::Core::Element* m_element = nullptr;
		unsigned m_document_id = 0;
		std::string m_control_name;
	};
}
