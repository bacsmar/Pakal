#include "rocketgui/RocketGUI.h"

#include <Rocket/Core.h>
#include <Rocket/Controls.h>
#include <Rocket/Core/String.h>
#include <irrlicht.h>

#include "rocketgui/RocketSystemInterface.h"
#include "rocketgui/PakalRocketFileInterface.h"
#include "rocketgui/IrrRocketRenderer.h"
#include "IInputManager.h"
#include "RocketInput.h"
#include "GraphicsSystem.h"
#include "EventScheduler.h"
#include "ResourceManager.h"
#include "RocketUIComponents.h"

using namespace Pakal;


RocketUI::RocketUI(GraphicsSystem* renderInterface) 
	: m_graphics_system(renderInterface), 
	m_mouse_move_e(0), 
	m_mouse_released_e(0), 
	m_mouse_pressed_e(0), 
	m_text_entered_e(0), 
	m_key_down_e(0), 
	m_key_up_e(0)
{
}

RocketUI::~RocketUI()
{
	//dtor
}

bool RocketUI::load_document(unsigned id, const Path& resourcePath)
{
	return load_document_async(id, resourcePath)->result();
}

TaskPtr<bool> RocketUI::load_document_async(unsigned id, const Path& resourcePath)
{
	std::function<bool()> fn = [=]()
	{
		auto documentId = m_loaded_documents.find(id);

		if (documentId != m_loaded_documents.end())	
		{
			LOG_WARNING("{LibRocket UI} document already loaded id=%d path=%s", id, resourcePath.c_str());
			return false;
		}

		auto stream = ResourceMgr.open_read_resource(resourcePath);
		std::string documentStr;
		file_utils::read_to_string(*stream, documentStr);

		Rocket::Core::ElementDocument* document = RocketContext->LoadDocumentFromMemory(documentStr.c_str());

		if (document)
		{			
			m_loaded_documents[id] = document;
			document->RemoveReference();
			return true;
		}
		return false;
	};
	return EventScheduler::instance().execute_in_thread(fn, m_graphics_system->thread_id() );
}

bool RocketUI::unload_document(unsigned id)
{
	return unload_document_async(id)->result();
	
}

TaskPtr<bool> RocketUI::unload_document_async(unsigned id)
{
	std::function<bool()> fn = [=]()
	{
		auto documentId = m_loaded_documents.find(id);

		if (documentId == m_loaded_documents.end())
			return false;

		auto document = documentId->second;
		if (document)
		{
			RocketContext->UnloadDocument(document);
			m_loaded_documents.erase(id);
			return true;
		}
		return false;
	};
	return EventScheduler::instance().execute_in_thread(fn, m_graphics_system->thread_id());
}

void RocketUI::show_document(unsigned id, bool autoresize)
{
	auto documentId = m_loaded_documents.find(id);

	if (documentId == m_loaded_documents.end())
		return;

	auto document = documentId->second;

	if (document)
	{
		//document->Focus();
		document->Show();
	}

}

void RocketUI::hide_document(unsigned id)
{
	auto documentId = m_loaded_documents.find(id);
	if (documentId == m_loaded_documents.end())
		return;
	
	auto document = documentId->second;

	if (document)
	{
		document->Hide();
	}
}

bool RocketUI::load_font(const Path& resourcePath)
{
	return Rocket::Core::FontDatabase::LoadFontFace(resourcePath.c_str());
}

bool RocketUI::set_element_inner_text(unsigned documentId, const char* elementName, const char* value)
{
	Rocket::Core::Element* document;
	if (map_utils::try_get(m_loaded_documents, documentId, document))
	{
		Rocket::Core::Element* element = document->GetElementById(elementName);
		if (element != nullptr)
		{
			element->SetInnerRML(value);
			return true;
		}
	}
	return false;
}

bool RocketUI::set_element_inner_text(unsigned documentId, const char* elementName, const int value)
{
	Rocket::Core::Element* document;
	if (map_utils::try_get(m_loaded_documents, documentId, document))
	{
		Rocket::Core::Element* element = document->GetElementById(elementName);
		if (element != nullptr)
		{
			element->SetInnerRML(std::to_string(value).c_str());
			return true;
		}
	}
	return false;
}

bool RocketUI::set_element_inner_text(unsigned documentId, const char* elementName, const float value)
{
	Rocket::Core::Element* document;
	if (map_utils::try_get(m_loaded_documents, documentId, document))
	{
		Rocket::Core::Element* element = document->GetElementById(elementName);
		if (element != nullptr)
		{
			element->SetInnerRML(Rocket::Core::String(256, "%f", value).CString());
			return true;
		}
	}
	return false;
}

bool RocketUI::set_element_visibility(unsigned documentId, const char* elementName, const bool visible)
{
	Rocket::Core::Element* document;
	if (map_utils::try_get(m_loaded_documents, documentId, document))
	{
		Rocket::Core::Element* element = document->GetElementById(elementName);
		if (element != nullptr)
		{
			element->SetProperty("visibility", visible ? "visible" : "hidden" );
			return true;
		}
	}
	return false;
}

void RocketUI::set_element_class(unsigned documentId, const char* elementName, const char* value)
{	
	Rocket::Core::Element* document;
	if (map_utils::try_get(m_loaded_documents, documentId, document))
	{
		Rocket::Core::Element* element = document->GetElementById(elementName);
		if (element != nullptr)
		{
			element->SetClassNames(Rocket::Core::String(256, "%s", value).CString());
		}
	}
}

Rocket::Core::Element* RocketUI::get_element(unsigned documentId, const char* elementName)
{
	Rocket::Core::Element* document;
	if (map_utils::try_get(m_loaded_documents, documentId, document))
	{
		return document->GetElementById(elementName);
	}
	return nullptr;
}

void RocketUI::register_component_factories(std::vector<IComponentFactory*>& factories)
{
	LOG_INFO("[RocketUI] Registering Components");

	factories.emplace_back(CreateComponentFactory<UILabel, RocketLabel>(this));
	factories.emplace_back(CreateComponentFactory<UILayoutElement, RocketLayoutElement>(this));
}

void RocketUI::initialize()
{
	setup_render_interface();

	m_rocket_FS = new PakalRocketFileInterface();
	m_rocket_system_interface = new RocketSystemInterface();

	Rocket::Core::Shutdown();
	// Rocket initialisation.
	ASSERT(m_renderInterface != nullptr);
	Rocket::Core::SetRenderInterface(m_renderInterface);
	Rocket::Core::SetFileInterface(m_rocket_FS);
	Rocket::Core::SetSystemInterface(m_rocket_system_interface);		

	Rocket::Core::Initialise();
	Rocket::Controls::Initialise();	

	RocketContext = Rocket::Core::CreateContext("default", Rocket::Core::Vector2i(m_screen_width, m_screen_height));

	RocketInput::set_context(RocketContext);

	OSMgr.get_input_manager()->event_mouse_moved += {m_mouse_move_e, std::bind(&RocketInput::process_mouse_move, std::placeholders::_1)};
	OSMgr.get_input_manager()->event_mouse_released += {m_mouse_released_e, std::bind(&RocketInput::process_mouse_released, std::placeholders::_1)};
	OSMgr.get_input_manager()->event_mouse_pressed += {m_mouse_pressed_e, std::bind(&RocketInput::process_mouse_pressed, std::placeholders::_1)};

	OSMgr.get_input_manager()->event_text += {m_text_entered_e, std::bind(&RocketInput::process_text_input, std::placeholders::_1)};
	OSMgr.get_input_manager()->event_key_down += {m_key_down_e, std::bind(&RocketInput::process_key_down, std::placeholders::_1)};
	OSMgr.get_input_manager()->event_key_up += {m_key_up_e, std::bind(&RocketInput::process_key_up, std::placeholders::_1)};
}

void RocketUI::terminate()
{	
	OSMgr.get_input_manager()->event_mouse_pressed -= m_mouse_pressed_e;
	OSMgr.get_input_manager()->event_mouse_released -= m_mouse_released_e;
	OSMgr.get_input_manager()->event_mouse_moved -= m_mouse_move_e;

	OSMgr.get_input_manager()->event_text -= m_text_entered_e;
	OSMgr.get_input_manager()->event_key_down -= m_key_down_e;
	OSMgr.get_input_manager()->event_key_up -= m_key_up_e;

	m_loaded_documents.clear();

	RocketInput::set_context(nullptr);

	if (RocketContext) 
	{
		RocketContext->RemoveReference();
		RocketContext = nullptr;
	}
	Rocket::Core::Shutdown();
	SAFE_DEL( m_rocket_FS);
	SAFE_DEL(m_rocket_system_interface);
	SAFE_DEL(m_renderInterface);
}

void RocketUI::set_theme(const Path& themePath)
{
}
