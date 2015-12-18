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

using namespace Pakal;


RocketUI::RocketUI(GraphicsSystem* renderInterface) 
	: m_graphics_system(renderInterface)
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

		if (documentId != m_loaded_documents.end())	// document already loaded
			return false;

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
	return EventScheduler::instance().execute_in_thread(fn, m_graphics_system->thread_id())->result();
}

TaskPtr<bool> RocketUI::unload_document_async(unsigned id)
{
	return TaskUtils::from_result(false);
}

void RocketUI::display_document(unsigned id, bool autoresize)
{
	auto documentId = m_loaded_documents.find(id);

	if (documentId == m_loaded_documents.end())
		return;

	auto document = documentId->second;

	if (document)
	{

		document->Focus();
		document->Show();
	}
}

void RocketUI::conceal_document(unsigned id)
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

bool RocketUI::set_element_inner_text(const char* documentId, const char* elementName, const char* value)
{
	if (RocketContext->GetDocument(documentId))
	{
		Rocket::Core::Element* element = RocketContext->GetDocument(documentId)->GetElementById(elementName);
		if (element != nullptr)
		{
			element->SetInnerRML(Rocket::Core::String(256, "%s", value).CString());
			return true;
		}
	}
	return false;
}
bool RocketUI::set_element_inner_text(const char* documentId, const char* elementName, const int value)
{
	if (RocketContext->GetDocument(documentId))
	{
		Rocket::Core::Element* element = RocketContext->GetDocument(documentId)->GetElementById(elementName);
		if (element != nullptr)
		{
			element->SetInnerRML(Rocket::Core::String(256, "%d", value).CString());
			return true;
		}
	}
	return false;
}
bool RocketUI::set_element_inner_text(const char* documentId, const char* elementName, const float value)
{
	if (RocketContext->GetDocument(documentId))
	{
		Rocket::Core::Element* element = RocketContext->GetDocument(documentId)->GetElementById(elementName);
		if (element != nullptr)
		{
			element->SetInnerRML(Rocket::Core::String(256, "%.2f", value).CString());
			return true;
		}
	}
	return false;
}


void RocketUI::set_element_class(const char* documentId, const char* elementName, const char* value)
{
	if (RocketContext->GetDocument(documentId))
	{
		Rocket::Core::Element* element = RocketContext->GetDocument(documentId)->GetElementById(elementName);
		if (element != nullptr)
		{

			element->SetClassNames(Rocket::Core::String(256, "%s", value).CString());
		}
	}
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

	m_mouse_move_e	   = OSMgr.get_input_manager()->event_mouse_moved.add_listener(std::bind(&RocketInput::process_mouse_move,std::placeholders::_1));
	m_mouse_released_e = OSMgr.get_input_manager()->event_mouse_released.add_listener(std::bind(&RocketInput::process_mouse_released,std::placeholders::_1));
	m_mouse_pressed_e  = OSMgr.get_input_manager()->event_mouse_pressed.add_listener(std::bind(&RocketInput::process_mouse_pressed,std::placeholders::_1));

}

void RocketUI::terminate()
{	
	OSMgr.get_input_manager()->event_mouse_pressed.remove_listener(m_mouse_pressed_e);
	OSMgr.get_input_manager()->event_mouse_released.remove_listener(m_mouse_released_e);
	OSMgr.get_input_manager()->event_mouse_moved.remove_listener(m_mouse_move_e);

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

bool RocketUI::set_element_visibility(const char* documentId, const char* elementName, bool visible)
{
	if (RocketContext->GetDocument(documentId))
	{
		Rocket::Core::Element* element = RocketContext->GetDocument(documentId)->GetElementById(elementName);
		if (element != nullptr)
		{
			if (!visible)
				element->SetProperty("visibility", "hidden");
			else
				element->SetProperty("visibility", "visible");

			return true;
		}
	}
	return false;
}