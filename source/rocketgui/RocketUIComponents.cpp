#include "RocketUIComponents.h"
#include "LogMgr.h"
#include "Rocket/Core/Element.h"

Pakal::RocketLabel::RocketLabel(RocketUI* manager) : m_rocket_ui(manager)
{
}

void Pakal::RocketLabel::set_ui_control(unsigned int documentId, const std::string& controlName)
{
	m_document_id = documentId;
	m_control_name = controlName;
	m_element = m_rocket_ui->get_element(m_document_id, m_control_name.c_str());
	ASSERT(m_element);
	if(m_element == nullptr)
		LOG_ERROR("the control %s doesn't exists in %d", controlName.c_str(), documentId);
}

void Pakal::RocketLabel::set_text(const std::string& text)
{
	if (m_element)
		m_element->SetInnerRML(text.c_str());
	else
		LOG_ERROR("the control doesn't exists");	
}

void Pakal::RocketLabel::set_visible(bool visible)
{
	if (m_element)
		m_element->SetProperty("visibility", visible ? "visible" : "hidden");
}

void Pakal::RocketLabel::set_color(unsigned color) const
{
	char buffer[24];
	sprintf(buffer, "#%06X", color);
	if (m_element)
		m_element->SetProperty("background-color", buffer);
}

void Pakal::RocketLabel::set_font_color(unsigned color) const
{
	char buffer[24];
	sprintf(buffer, "#%06X", color);
	if (m_element)
		m_element->SetProperty("color", buffer);
}

void Pakal::RocketLabel::set_width(unsigned width) const
{	
	char buffer[24];
	if (m_element)
		m_element->SetProperty("width", itoa(width,buffer, 10));
}
