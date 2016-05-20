#include "RocketUIComponents.h"
#include "LogMgr.h"
#include "Rocket/Core/Element.h"


Pakal::RocketLabel::RocketLabel(RocketUI* manager) : m_rocket_ui(manager)
{
}

Pakal::RocketLabel::~RocketLabel()
{
	evt_ui.clear();
	unsuscribe_to_events();
}

void Pakal::RocketLabel::ProcessEvent(Rocket::Core::Event& event)
{
	evt_ui.notify({ *this, event.GetType().CString()});
}

void Pakal::RocketLabel::enable_ui_events(bool enable)
{
	enable ? suscribe_to_events() : unsuscribe_to_events();
}

void Pakal::RocketLabel::set_ui_control(unsigned int documentId, const std::string& controlName)
{
	unsuscribe_to_events();		

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

void Pakal::RocketLabel::set_width_percent(unsigned width) const
{	
	auto buffer = std::to_string(width)+"%";
	if (m_element)
		m_element->SetProperty("width", buffer.c_str());
}

int Pakal::RocketLabel::get_width() const
{	
	if (m_element)
	{
		auto width = m_element->GetProperty("width");
		return atoi(width->ToString().CString());
	}
	return 0;
}

void Pakal::RocketLabel::terminate()
{
	evt_ui.clear();
	unsuscribe_to_events();
	m_element = nullptr;
}

void Pakal::RocketLabel::suscribe_to_events()
{
	if (m_element == nullptr)
		return;
	m_element->AddEventListener("click", this);
	m_element->AddEventListener("focus", this);
	m_element->AddEventListener("mouseover", this);
	m_element->AddEventListener("mouseout", this);
}

void Pakal::RocketLabel::unsuscribe_to_events()
{
	if (m_element == nullptr)
		return;	

	m_element->RemoveEventListener("click", this);
	m_element->RemoveEventListener("focus", this);
	m_element->RemoveEventListener("mouseover", this);
	m_element->RemoveEventListener("mouseout", this);
}
