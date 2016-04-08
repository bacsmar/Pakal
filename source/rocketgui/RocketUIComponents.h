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
	private:
		RocketUI* m_rocket_ui = nullptr;
		unsigned m_document_id = 0;
		std::string m_control_name;
	};
}
