#include "RocketUIComponents.h"

Pakal::RocketLabel::RocketLabel(RocketUI* manager) : m_rocket_ui(manager)
{
}

void Pakal::RocketLabel::set_ui_control(unsigned int documentId, const std::string& controlName)
{
	m_document_id = documentId;
	m_control_name = controlName;
}

void Pakal::RocketLabel::set_text(const std::string& text)
{
	m_rocket_ui->set_element_inner_text(m_document_id, m_control_name.c_str(), text.c_str());
}
