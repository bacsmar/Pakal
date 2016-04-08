
#pragma once

#include "IUIManager.h"
#include "Event.h"

namespace Rocket
{
    namespace Core
    {
	    class Element;
	    class Context;
		class RenderInterface;
		class FileInterface;
		class ElementDocument;
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
		TaskPtr<bool> unload_document_async(unsigned id) override;
		void show_document(unsigned id, bool autoresize) override;
		void hide_document(unsigned id) override;
		bool load_font(const Path& resourcePath) override;

		//void draw_ui() override;

		void set_theme(const Path& themePath) override;
		bool set_element_inner_text(unsigned documentId, const char* elementName, const char* value);
		bool set_element_inner_text(unsigned documentId, const char* elementName, const int value);
		bool set_element_inner_text(unsigned documentId, const char* elementName, const float value);
		bool set_element_visibility(unsigned documentId, const char* elementName, const bool visible);
		void set_element_class(unsigned documentId, const char* elementName, const char* value);

		Rocket::Core::Element* get_element(unsigned documentId, const char* elementName);
		
	protected:
		void register_component_factories(std::vector<IComponentFactory*>& factories) override;
		virtual void setup_render_interface() = 0;
		void initialize() override final;
		void terminate() override final;

		Rocket::Core::Context*			RocketContext = nullptr;
		unsigned						m_screen_width = 0, m_screen_height = 0;
		Rocket::Core::FileInterface*	m_rocket_FS = nullptr;
		Rocket::Core::RenderInterface*	m_renderInterface = nullptr;
		RocketSystemInterface*			m_rocket_system_interface = nullptr;		
		GraphicsSystem*					m_graphics_system = nullptr;
		
		std::unordered_map<unsigned, Rocket::Core::ElementDocument*> m_loaded_documents;

		EventId m_mouse_move_e;
		EventId m_mouse_released_e;
		EventId m_mouse_pressed_e;
	};
}