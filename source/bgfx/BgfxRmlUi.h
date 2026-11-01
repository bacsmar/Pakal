#pragma once

#include "Config.h"
#include "IUIManager.h"
#include "EventArgs.h"

#include <bgfx/bgfx.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Rml { class Context; class ElementDocument; class Element; }

namespace Pakal
{
	class BgfxGraphicsSystem;
	class IInputManager;

	// The game's interface: RmlUi (documents in HTML/CSS-like files, .rml and .rcss) drawn with bgfx in its own view over the
	// picture. Documents are loaded by id; the game changes their elements by id and hears the clicks. Paths are relative to
	// the working folder or to the resource root. Sizes in "dp" scale with the screen (1 dp = 1 px at 720 lines).
	class _PAKALExport BgfxRmlUi final : public IUIManager
	{
	public:
		explicit BgfxRmlUi(BgfxGraphicsSystem* graphics);
		~BgfxRmlUi() override;

		void register_component_factories(std::vector<IComponentFactory*>& factories) override { (void)factories; }

		TaskPtr<bool> load_document_async(unsigned id, const Path& resourcePath) override;
		TaskPtr<bool> unload_document_async(unsigned id) override;
		bool load_document(unsigned id, const Path& resourcePath) override;
		bool unload_document(unsigned id) override;
		void show_document(unsigned id, bool autoresize = true) override;
		void hide_document(unsigned id) override;
		void bring_to_front(unsigned id) override;
		bool load_font(const Path& resourcePath) override;
		bool load_fallback_font(const Path& resourcePath) override;
		void set_theme(const Path& themePath) override;

		void draw_ui() override;
		void initialize() override;
		void terminate() override;

		bool is_document_visible(unsigned id) const override;
		void set_text(unsigned document, const std::string& element, const std::string& text) override;
		void set_markup(unsigned document, const std::string& element, const std::string& markup) override;
		void set_class(unsigned document, const std::string& element, const std::string& name, bool on) override;
		void set_property(unsigned document, const std::string& element, const std::string& name, const std::string& value) override;
		void set_attribute(unsigned document, const std::string& element, const std::string& name, const std::string& value) override;
		bool create_data_model(const std::string& model, const std::string& json) override;
		void remove_data_model(const std::string& model) override;
		void set_data(const std::string& model, const std::string& variable, const std::string& json) override;
		void add_data_event(const std::string& model, const std::string& name, DataEventCallback callback) override;
		void set_strings(const std::map<std::string, std::string>& strings) override;
		unsigned add_click_listener(unsigned document, ClickCallback callback) override;
		void remove_click_listener(unsigned listener) override;
		bool is_over_ui(int x, int y) const override;
		void pointer(int x, int y, bool down) override;
		void pointer_move(int x, int y) override;
		void touch(int id, int x, int y, int phase) override;

		class Backend;
		class ClickListener;
		struct DataModel;

	private:
		Rml::Element* find(unsigned document, const std::string& element) const;
		void attach_input(IInputManager* input);
		void detach_input();
		void feed_events();

		struct InputEvent
		{
			enum class Type { Move, Button, Wheel, Key, Text, Touch } type;
			int x = 0, y = 0, button = 0;
			bool down = false;
			float wheel = 0.0f;
			Key key = Key::Unknown;
			unsigned modifiers = 0;
			unsigned text = 0;
			int touch = 0, phase = 0; // a finger: its id, and 0 down, 1 moved, 2 up
		};
		void push(const InputEvent& event);

		BgfxGraphicsSystem* m_graphics;
		std::unique_ptr<Backend> m_backend;
		Rml::Context* m_context = nullptr;
		std::map<unsigned, Rml::ElementDocument*> m_documents;
		struct Listener { unsigned document; std::unique_ptr<ClickListener> listener; };
		std::map<unsigned, Listener> m_listeners;
		unsigned m_next_listener = 1;
		unsigned m_width = 0, m_height = 0;
		bool m_initialized = false;

		std::mutex m_mutex;
		std::vector<InputEvent> m_events;
		// The game changes the documents from its thread and the graphics system draws them from its own (Android)
		mutable std::recursive_mutex m_lock;
		IInputManager* m_input = nullptr;
		unsigned m_ids[7] = {};
		std::map<std::string, std::unique_ptr<DataModel>> m_models;
	};
}
