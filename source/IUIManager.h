#pragma once

#include "Config.h"
#include "TaskFwd.h"
#include "PakalPath.h"
#include "IComponentProvider.h"
#include "BuildIdentity.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

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
		// A document from its markup (the engine's own screens: the splash); its relative paths are the resources' root's
		virtual bool load_document_from_text(unsigned id, const std::string& markup) { (void)id; (void)markup; return false; }

		virtual void show_document(unsigned id, bool autoresize = true) = 0;
		virtual void hide_document(unsigned id) = 0;
		// Over the other documents (a panel opened over a screen)
		virtual void bring_to_front(unsigned id) { (void)id; }
		// A loaded document read again from its files, in place (docs/UI_RELOAD.md): the same id, shown or hidden as it was, its
		// click listeners, the focus, what the game set on its elements (set_text, set_class...) put back, and the data models
		// bound again. In dev and --tools builds a document reloads by itself when its .rml or a style sheet it links changes;
		// a game calls these after it changed what the documents read as they load (set_strings: their texts). The game's thread.
		virtual bool reload_document(unsigned id) { (void)id; return false; }
		virtual void reload_documents() {}

		virtual bool load_font(const Path& resourcePath) = 0;
		// A font for the characters the documents' own fonts lack (another script: Chinese, Cyrillic)
		virtual bool load_fallback_font(const Path& resourcePath) { return load_font(resourcePath); }
		virtual void set_theme(const Path& themePath) = 0;

		// The build's label (BuildIdentity::label, C-ID-7), drawn over every document in a corner, never taking a click or the
		// focus. The engine sets it; an empty text hides it. It uses the first font loaded (load_font)
		virtual void set_build_label(const std::string& text, BuildLabelCorner corner) { (void)text; (void)corner; }
		// The label while this manager cannot draw it (no font loaded yet, or none at all): the graphics system draws it as
		// debug text instead. Empty when it is drawn here, or hidden
		virtual std::string undrawn_build_label(BuildLabelCorner* corner) const { (void)corner; return {}; }
		virtual void draw_ui() = 0;
		virtual void initialize() = 0;
		virtual void terminate() = 0;

		// The documents loaded now, by id: the file each came from, its title, and whether it is shown (the QA tools' "state").
		// From any thread.
		struct DocumentInfo
		{
			unsigned id = 0;
			std::string source;
			std::string title;
			bool visible = false;
		};
		virtual std::vector<DocumentInfo> documents() const { return {}; }

		// --- Working with the elements of a loaded document (by the element's id); nothing happens if they do not exist ---
		virtual bool is_document_visible(unsigned id) const { (void)id; return false; }
		// The element's content as plain text (escaped) or as markup of the UI language
		virtual void set_text(unsigned document, const std::string& element, const std::string& text) { (void)document; (void)element; (void)text; }
		virtual void set_markup(unsigned document, const std::string& element, const std::string& markup) { (void)document; (void)element; (void)markup; }
		virtual void set_class(unsigned document, const std::string& element, const std::string& name, bool on) { (void)document; (void)element; (void)name; (void)on; }
		// A style property ("width", "50%") or an attribute ("src", "images/x.png")
		virtual void set_property(unsigned document, const std::string& element, const std::string& name, const std::string& value) { (void)document; (void)element; (void)name; (void)value; }
		virtual void set_attribute(unsigned document, const std::string& element, const std::string& name, const std::string& value) { (void)document; (void)element; (void)name; (void)value; }
		// A scrolling element (overflow-y: auto) moved by 'dp' (down positive), for keys and pads (a finger drags it itself); and
		// the one that holds 'element' scrolled until it is in sight
		virtual void scroll_by(unsigned document, const std::string& element, float dp) { (void)document; (void)element; (void)dp; }
		virtual void scroll_into_view(unsigned document, const std::string& element) { (void)document; (void)element; }
		// Keys and gamepads through a document's buttons, as a finger or the mouse would press them (UiNav reads the keys for
		// it). The buttons are the elements its style makes focusable (tab-index: auto) that are shown. navigate moves the
		// focus to the nearest one that way (the first one, when none has it yet), shown with :focus-visible and scrolled into
		// sight; activate clicks the focused one. While an element of class "nav-trap" is shown (a dialog over the screen), only
		// the buttons inside it take the focus.
		enum class NavDirection { Up, Down, Left, Right };
		virtual void navigate(unsigned document, NavDirection direction) { (void)document; (void)direction; }
		virtual void activate(unsigned document) { (void)document; }
		// The first button takes the focus when none of the document's has it (a screen that just came up, a dialog that
		// opened or closed): with keys or a pad, one button is always lit and Return (a pad's A) always presses one
		virtual void focus_first(unsigned document) { (void)document; }

		// Clicks (and taps) in a document: the callback gets the id of the clicked element, or of its nearest parent that has one.
		// Returns an id for remove_click_listener.
		using ClickCallback = std::function<void(const std::string& element)>;
		virtual unsigned add_click_listener(unsigned document, ClickCallback callback) { (void)document; (void)callback; return 0; }
		virtual void remove_click_listener(unsigned listener) { (void)listener; }

		// --- Data the documents show (the model of a model-view: the views are the documents, which bind to it) ---
		// A model by name whose variables are the top members of a JSON object: numbers, text, booleans, lists and records,
		// nested as deep as needed. The documents read it with the UI language's bindings ({{money}}, data-for, data-if,
		// data-class-...). Make it before loading a document that uses it (<body data-model="name">); its variables are the
		// ones in 'json' and each keeps its kind (a list stays a list).
		virtual bool create_data_model(const std::string& model, const std::string& json) { (void)model; (void)json; return false; }
		// The model gone (unload the documents that use it first)
		virtual void remove_data_model(const std::string& model) { (void)model; }
		// A variable of a model given a new value (JSON text): what shows it updates
		virtual void set_data(const std::string& model, const std::string& variable, const std::string& json) { (void)model; (void)variable; (void)json; }
		// An event the documents raise by name, with its arguments (data-event-click="buy(item.id)"): the callback gets
		// them as text, on the thread that draws the interface. Registered with the model, before its documents load.
		using DataEventCallback = std::function<void(const std::vector<std::string>& arguments)>;
		virtual void add_data_event(const std::string& model, const std::string& name, DataEventCallback callback) { (void)model; (void)name; (void)callback; }
		// The documents' texts by key (languages): a text in a document that is a key of the table shows the table's text
		virtual void set_strings(const std::map<std::string, std::string>& strings) { (void)strings; }

		// Whether a point of the screen (pixels) is on an element that takes input; the game then leaves that touch or click alone
		virtual bool is_over_ui(int x, int y) const { (void)x; (void)y; return false; }
		// Screen pixels per dp: interfaces are laid out for 1280x720 dp, fit whole on the screen (by its height when it is 16:9 or
		// wider, by its width on a narrower one). What a game drawing its own controls (a touch stick) sizes them by. 1 without a UI.
		virtual float dp_ratio() const { return 1.0f; }
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