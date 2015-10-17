#include "rocketgui/RocketGUI.h"

#include <Rocket/Core.h>
#include <Rocket/Controls.h>
#include <Rocket/Core/String.h>
#include <irrlicht.h>

#include "rocketgui/RocketSystemInterface.h"
#include "rocketgui/PakalRocketFileInterface.h"
#include "rocketgui/IrrRocketRenderer.h"
#include "GraphicsSystem.h"

using namespace Pakal;

bool RocketUI::load_document(const char* documentPath, bool show, bool autoresize)
{
	m_graphics_system->execute_block([=]()
	{
		std::string base;
		base.append(documentPath);

		Rocket::Core::ElementDocument* document = RocketContext->LoadDocument(base.c_str());

		if (document)
		{
			if (show)
			{
				document->Focus();
				document->Show();
			}
			document->RemoveReference();
		}
	});

	//std::string base;
	//base.append(documentPath);

	//Rocket::Core::ElementDocument* document = RocketContext->LoadDocument(base.c_str());

	//if (document)
	//{
	//	if (show)
	//	{
	//		document->Focus();
	//		document->Show();
	//	}
	//	document->RemoveReference();

	//	return true;
	//}

	return false;
}

bool RocketUI::close_document(const char* documentId)
{
	Rocket::Core::ElementDocument* document = RocketContext->GetDocument(documentId);

	if (document)
	{
		document->Close();

		return true;
	}

	return false;
}

bool RocketUI::set_document_visible(const char* documentId, const bool visible)
{
	Rocket::Core::ElementDocument* document = RocketContext->GetDocument(documentId);

	if (document)
	{
		if (visible)
		{
			document->Focus();
			document->Show();
		}
		else
			document->Hide();

		return true;
	}
	return false;
}

bool RocketUI::load_font(const char* path)
{
	return Rocket::Core::FontDatabase::LoadFontFace(path);
}

RocketUI::RocketUI(GraphicsSystem* renderInterface, IInputManager* input_manager) 
	: m_graphics_system(renderInterface), m_input_manager(input_manager)
{
}

RocketUI::~RocketUI()
{
	//dtor

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
}

void RocketUI::terminate()
{
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