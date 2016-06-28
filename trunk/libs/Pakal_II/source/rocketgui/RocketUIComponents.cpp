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
	if (m_element)
		m_element->SetProperty("width", std::to_string(width).c_str());
}

void Pakal::RocketLabel::set_width_percent(unsigned width) const
{	
	auto buffer = std::to_string(width) + '%';
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
//------------------------------------------------------------------------------------------------------------
Pakal::RocketLayoutElement::RocketLayoutElement(RocketUI* manager) : m_rocket_ui(manager)
{

}

Pakal::RocketLayoutElement::~RocketLayoutElement()
{

	evt_ui.clear();
	unsuscribe_to_events();
}

void Pakal::RocketLayoutElement::ProcessEvent(Rocket::Core::Event& event)
{
	evt_ui.notify({ *this, event.GetType().CString() });
}

void Pakal::RocketLayoutElement::enable_ui_events(bool enable)
{
	


	enable ?  suscribe_to_events() : unsuscribe_to_events();
}

void Pakal::RocketLayoutElement::set_visible(bool visible)
{
	if (m_element)
		m_element->SetProperty("visibility", visible ? "visible" : "hidden");
}

void Pakal::RocketLayoutElement::set_ui_control(unsigned documentId, const std::string& controlName)
{
	unsuscribe_to_events();
	
	m_document_id = documentId;
	m_control_name = controlName;
	m_element = m_rocket_ui->get_element(m_document_id, m_control_name.c_str());
	
	if (m_element == nullptr)
		LOG_ERROR("the control %s doesn't exists in %d", controlName.c_str(), documentId);
	ASSERT(m_element);
}

bool Pakal::RocketLayoutElement::add_element_inside(UILayoutElement* element_to_add) const
{
	if (element_to_add->get_type() == Pakal::TypeInfo::get<RocketLayoutElement>())
	{
		RocketLayoutElement* val = static_cast<RocketLayoutElement*>(element_to_add);
		val->set_document_id(m_document_id);
		m_element->AppendChild(val->get_element());		
		return true;
	}			
	return false;
}

void Pakal::RocketLayoutElement::remove_inside_elements() const
{
	if(m_element)
	{
		m_element->SetInnerRML("");
	}
}

bool Pakal::RocketLayoutElement::add_element_after(UILayoutElement* element_to_add) const
{

	if (element_to_add->get_type() == Pakal::TypeInfo::get<RocketLayoutElement>())
	{
		RocketLayoutElement* element_tmp = static_cast<RocketLayoutElement*>(element_to_add);
		auto sibiling = m_element->GetNextSibling();
		sibiling->InsertBefore(element_tmp->get_element(), sibiling);
		return true;
	}

	return false;
}

void Pakal::RocketLayoutElement::load_style(const std::string& theme_name) const
{

	if (m_element)
	{
	//m_element->SetClass(theme_name.c_str(), true);
	//m_element->SetProperty("class", theme_name.c_str());
	m_element->SetAttribute("class", theme_name.c_str());
	}
}

void Pakal::RocketLayoutElement::set_color(unsigned color) const
{

	
	char buffer[24];
	sprintf(buffer, "#%06X", color);
	if (m_element)
		m_element->SetProperty("background-color", buffer);
}

void Pakal::RocketLayoutElement::set_font_color(unsigned color) const
{
	char buffer[24];
	sprintf(buffer, "#%06X", color);
	if (m_element)
		m_element->SetAttribute("font-color", buffer);
		//m_element->SetProperty("color", buffer);
}

void Pakal::RocketLayoutElement::set_width(int width) const
{
	if (m_element)
		m_element->SetProperty("width", std::to_string(width).c_str());
}

void Pakal::RocketLayoutElement::set_with_percentage(float width) const
{
	if (m_element) {
		auto inner_width = m_element->GetClientWidth();
		set_width((width * inner_width) / 100.f);
	}
}

void Pakal::RocketLayoutElement::set_heigth(int pixels) const
{	
	if (m_element)
		m_element->SetProperty("heigth", std::to_string(pixels).c_str());
}

void Pakal::RocketLayoutElement::set_heigth_percentage(float container_percentage) const
{
	if (m_element) {
		auto inner_height = m_element->GetClientHeight();
		set_heigth((container_percentage*inner_height) * 0.01f);
	}
}

void Pakal::RocketLayoutElement::set_padding(int pixels) const
{
	if (m_element) {
		m_element->SetAttribute("padding", std::to_string(pixels).c_str());
	}
}

void Pakal::RocketLayoutElement::set_padding_percentage(float containter_percentage) const
{
	auto buffer = std::to_string(containter_percentage) + '%';
	if (m_element) {
		m_element->SetAttribute("padding", buffer);
	}
}

void Pakal::RocketLayoutElement::clone_on(UILayoutElement* target) const
{
	//class Bypass : Rocket::Core::Element
	//{	
	//	Rocket::Core::Element* me;
	//public:
	//	Bypass(const Rocket::Core::Element& element) : Element(element) {}
	//	void get_rml(Rocket::Core::String& buffer)
	//	{
	//		this->GetRML(buffer);
	//	}
	//	~Bypass(){}
	//};

	//Rocket::Core::String buffer;
	//Bypass *p = new Bypass(*m_element);
	//p->get_rml(buffer);
	//delete p;

	if (m_element) {
		if (target->get_type() == Pakal::TypeInfo::get<RocketLayoutElement>())
		{
		auto cloned_rocket_element = m_element->Clone();
		RocketLayoutElement* casted_element = static_cast<RocketLayoutElement*>(target);

			casted_element->set_rocket_element(cloned_rocket_element, true);
			casted_element->set_document_id(m_document_id);
			casted_element->set_rocket_ui(m_rocket_ui);
			casted_element->set_control_name(m_control_name);
		}
	}
	
}

void Pakal::RocketLayoutElement::set_id_value(const std::string&  newId)
{
	m_element->SetId(newId.c_str());
	m_control_name = newId;
}

std::string Pakal::RocketLayoutElement::get_controled_element_name() const
{
	return m_control_name;
}

void Pakal::RocketLayoutElement::terminate()
{
	evt_ui.clear();
	unsuscribe_to_events();
	if (m_cloned)
		m_element->RemoveReference();

	m_element = nullptr;
}

void Pakal::RocketLayoutElement::suscribe_to_events()
{
	if (m_element == nullptr||already_subscribed_to_events)
		return;
	m_element->AddEventListener("click", this);
	m_element->AddEventListener("focus", this);
	m_element->AddEventListener("mouseover", this);
	m_element->AddEventListener("mouseout", this);
	already_subscribed_to_events = true;
}

void Pakal::RocketLayoutElement::unsuscribe_to_events()
{
	if (m_element == nullptr)
		return;

	m_element->RemoveEventListener("click", this);
	m_element->RemoveEventListener("focus", this);
	m_element->RemoveEventListener("mouseover", this);
	m_element->RemoveEventListener("mouseout", this);
	already_subscribed_to_events = false;
}

Rocket::Core::Element* Pakal::RocketLayoutElement::get_element() const
{
	return m_element;
}

void Pakal::RocketLayoutElement::set_rocket_element(Rocket::Core::Element* newElement, bool cloned)
{
	m_element = newElement;
	m_cloned = cloned;
}

void Pakal::RocketLayoutElement::set_rocket_ui(RocketUI* rocketUi)
{
	m_rocket_ui = rocketUi;
}

void Pakal::RocketLayoutElement::set_document_id(unsigned documentId)
{
	m_document_id = documentId;
}

void Pakal::RocketLayoutElement::set_control_name(const std::string& controlName)
{
	m_control_name = controlName;
}

unsigned Pakal::RocketLayoutElement::document_id() const
{
	return m_document_id;
}
