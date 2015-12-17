
#pragma once

#include "IUIManager.h"
#include <Pakal_II/source/Event.h>

namespace Rocket
{
    namespace Core
    {
        class Context;
		class RenderInterface;
		class FileInterface;
    }
}

class RocketSystemInterface;

namespace Pakal 
{
	class GraphicsSystem;
	class IInputManager;
	class ResourceManager;

	class _PAKALExport RocketUI : public IUIManager
	{			
	public:		
		/** Default constructor */		
		explicit RocketUI(GraphicsSystem *renderInterface);
		/** Default destructor */
		virtual ~RocketUI();		

		bool load_document(unsigned id, const Path& resourcePath) override;
		TaskPtr<bool> load_document_async(unsigned id, const Path& resourcePath) override;
		bool unload_document(unsigned id) override;
		void display_document(unsigned id, bool autoresize) override;
		void conceal_document(unsigned id) override;
		bool load_font(const Path& resourcePath) override;
		//void draw_ui() override;		

		bool set_element_inner_text(const char* documentId, const char* elementName, const char* value);
		bool set_element_inner_text(const char* documentId, const char* elementName, const int value);
		bool set_element_inner_text(const char* documentId, const char* elementName, const float value);
		bool set_element_visibility(const char* documentId, const char* elementName, const bool visible);		
		void set_element_class(const char* documentId, const char* elementName, const char* value);			
		
	protected:
		virtual void setup_render_interface() = 0;
		void initialize() override final;
		void terminate() override final;

		Rocket::Core::Context*			RocketContext = nullptr;
		unsigned						m_screen_width = 0, m_screen_height = 0;
		Rocket::Core::FileInterface*	m_rocket_FS = nullptr;
		Rocket::Core::RenderInterface*	m_renderInterface = nullptr;
		RocketSystemInterface*			m_rocket_system_interface = nullptr;		
		GraphicsSystem*					m_graphics_system = nullptr;

		std::unordered_map<unsigned, std::string> m_loaded_documents;

		ulonglong m_mouse_move_e;
		ulonglong m_mouse_released_e;
		ulonglong m_mouse_pressed_e;
	};
}