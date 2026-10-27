#pragma once

#include "Config.h"
#include "TaskFwd.h"
#include "PakalPath.h"
#include "IComponentProvider.h"

#include <functional>
#include <string>

namespace Pakal
{
	class _PAKALExport IUIManager: public IComponentProvider
	{
	public:
		virtual ~IUIManager() {};

		virtual TaskPtr<bool> load_document_async(unsigned id, const Path& resourcePath) = 0;
		virtual TaskPtr<bool> unload_document_async(unsigned id) = 0;

		virtual bool load_document(unsigned id, const Path& resourcePath) = 0;
		virtual bool unload_document(unsigned id) = 0;

		virtual void show_document(unsigned id, bool autoresize = true) = 0;
		virtual void hide_document(unsigned id) = 0;

		virtual bool load_font(const Path& resourcePath) = 0;
		virtual void set_theme(const Path& themePath) = 0;

		virtual void draw_ui() = 0;
		virtual void initialize() = 0;
		virtual void terminate() = 0;

		// --- Working with the elements of a loaded document (by the element's id); nothing happens if they do not exist ---
		virtual bool is_document_visible(unsigned id) const { (void)id; return false; }
		// The element's content as plain text (escaped) or as markup of the UI language
		virtual void set_text(unsigned document, const std::string& element, const std::string& text) { (void)document; (void)element; (void)text; }
		virtual void set_markup(unsigned document, const std::string& element, const std::string& markup) { (void)document; (void)element; (void)markup; }
		virtual void set_class(unsigned document, const std::string& element, const std::string& name, bool on) { (void)document; (void)element; (void)name; (void)on; }
		// A style property ("width", "50%") or an attribute ("src", "images/x.png")
		virtual void set_property(unsigned document, const std::string& element, const std::string& name, const std::string& value) { (void)document; (void)element; (void)name; (void)value; }
		virtual void set_attribute(unsigned document, const std::string& element, const std::string& name, const std::string& value) { (void)document; (void)element; (void)name; (void)value; }

		// Clicks (and taps) in a document: the callback gets the id of the clicked element, or of its nearest parent that has one.
		// Returns an id for remove_click_listener.
		using ClickCallback = std::function<void(const std::string& element)>;
		virtual unsigned add_click_listener(unsigned document, ClickCallback callback) { (void)document; (void)callback; return 0; }
		virtual void remove_click_listener(unsigned listener) { (void)listener; }

		// Whether a point of the screen (pixels) is on an element that takes input; the game then leaves that touch or click alone
		virtual bool is_over_ui(int x, int y) const { (void)x; (void)y; return false; }
		// Touch screens: the platform sends the touches that land on the interface
		virtual void pointer(int x, int y, bool down) { (void)x; (void)y; (void)down; }
		virtual void pointer_move(int x, int y) { (void)x; (void)y; }
		// A finger on a touch screen (pixels of the back buffer): phase 0 down, 1 moved, 2 up. A drag scrolls what scrolls
		// (a list), a tap clicks. The default treats it as the mouse.
		virtual void touch(int id, int x, int y, int phase)
		{
			(void)id;
			if (phase == 1) pointer_move(x, y);
			else pointer(x, y, phase == 0);
		}
	};
}