#include "BgfxRmlUi.h"

#include "BgfxGraphicsSystem.h"
#include "IInputManager.h"
#include "LogMgr.h"
#include "OSManager.h"
#include "ResourceManager.h"
#include "Task.h"

#include <RmlUi/Core.h>
#include <bimg/decode.h>
#include <bx/allocator.h>
#include <bx/math.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <fstream>

namespace Pakal
{
	namespace
	{
		// The file's path as given, else under the resource root (Android: the app's storage)
		std::string resolve(const std::string& path)
		{
			if (FILE* file = std::fopen(path.c_str(), "rb"))
			{
				std::fclose(file);
				return path;
			}
			const std::string& root = ResourceMgr.get_root_path();
			if (!root.empty()) return root + "/" + path;
			return path;
		}

		Rml::Input::KeyIdentifier to_rml(Key key)
		{
			const int k = static_cast<int>(key);
			if (k >= static_cast<int>(Key::A) && k <= static_cast<int>(Key::Z)) return Rml::Input::KeyIdentifier(Rml::Input::KI_A + (k - static_cast<int>(Key::A)));
			if (k >= static_cast<int>(Key::Num0) && k <= static_cast<int>(Key::Num9)) return Rml::Input::KeyIdentifier(Rml::Input::KI_0 + (k - static_cast<int>(Key::Num0)));
			if (k >= static_cast<int>(Key::F1) && k <= static_cast<int>(Key::F12)) return Rml::Input::KeyIdentifier(Rml::Input::KI_F1 + (k - static_cast<int>(Key::F1)));
			switch (key)
			{
			case Key::Space: return Rml::Input::KI_SPACE;
			case Key::Escape: return Rml::Input::KI_ESCAPE;
			case Key::Return: return Rml::Input::KI_RETURN;
			case Key::BackSpace: return Rml::Input::KI_BACK;
			case Key::Tab: return Rml::Input::KI_TAB;
			case Key::Delete: return Rml::Input::KI_DELETE;
			case Key::Home: return Rml::Input::KI_HOME;
			case Key::End: return Rml::Input::KI_END;
			case Key::PageUp: return Rml::Input::KI_PRIOR;
			case Key::PageDown: return Rml::Input::KI_NEXT;
			case Key::Left: return Rml::Input::KI_LEFT;
			case Key::Right: return Rml::Input::KI_RIGHT;
			case Key::Up: return Rml::Input::KI_UP;
			case Key::Down: return Rml::Input::KI_DOWN;
			default: return Rml::Input::KI_UNKNOWN;
			}
		}

		int to_rml_modifiers(unsigned modifiers)
		{
			int out = 0;
			if (modifiers & KeyArgs::KM_CTRL) out |= Rml::Input::KM_CTRL;
			if (modifiers & KeyArgs::KM_SHIFT) out |= Rml::Input::KM_SHIFT;
			if (modifiers & KeyArgs::KM_ALT) out |= Rml::Input::KM_ALT;
			return out;
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// RmlUi's render, system and file interfaces on bgfx and the engine
	// ---------------------------------------------------------------------------------------------------------------
	class BgfxRmlUi::Backend final : public Rml::RenderInterface, public Rml::SystemInterface, public Rml::FileInterface
	{
	public:
		explicit Backend(BgfxGraphicsSystem* graphics) : m_graphics(graphics)
		{
			m_start = std::chrono::steady_clock::now();
		}

		bool create(bgfx::ViewId view)
		{
			m_view = view;
			m_program = m_graphics->load_program("vs_ui", "fs_ui");
			if (!bgfx::isValid(m_program)) return false;
			m_layout.begin()
				.add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
				.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
				.end();
			m_u_offset = bgfx::createUniform("u_uiOffset", bgfx::UniformType::Vec4);
			m_s_texture = bgfx::createUniform("s_uiTexture", bgfx::UniformType::Sampler);
			const uint32_t white = 0xffffffff;
			m_white = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, 0, bgfx::copy(&white, 4));
			return true;
		}

		void destroy()
		{
			for (auto& [handle, geometry] : m_geometries)
			{
				bgfx::destroy(geometry.vertices);
				bgfx::destroy(geometry.indices);
			}
			m_geometries.clear();
			for (auto& [handle, texture] : m_textures) bgfx::destroy(texture);
			m_textures.clear();
			if (bgfx::isValid(m_white)) bgfx::destroy(m_white);
			if (bgfx::isValid(m_u_offset)) bgfx::destroy(m_u_offset);
			if (bgfx::isValid(m_s_texture)) bgfx::destroy(m_s_texture);
			if (bgfx::isValid(m_program)) bgfx::destroy(m_program);
			m_white = BGFX_INVALID_HANDLE;
			m_u_offset = m_s_texture = BGFX_INVALID_HANDLE;
			m_program = BGFX_INVALID_HANDLE;
		}

		// The view for this frame: the back buffer, in pixels with (0,0) at the top left
		void begin_frame(unsigned width, unsigned height)
		{
			float ortho[16];
			bx::mtxOrtho(ortho, 0.0f, float(width), float(height), 0.0f, -1.0f, 1.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
			bgfx::setViewName(m_view, "UI");
			bgfx::setViewMode(m_view, bgfx::ViewMode::Sequential);
			bgfx::setViewFrameBuffer(m_view, BGFX_INVALID_HANDLE); // after the upscale: sharp at the screen's size
			bgfx::setViewRect(m_view, 0, 0, uint16_t(width), uint16_t(height));
			bgfx::setViewTransform(m_view, nullptr, ortho);
			m_width = width;
			m_height = height;
			m_scissor_on = false;
			m_has_transform = false;
		}

		// --- RenderInterface ---
		Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices, Rml::Span<const int> indices) override
		{
			if (vertices.empty() || indices.empty()) return 0;
			Geometry geometry;
			geometry.vertices = bgfx::createVertexBuffer(bgfx::copy(vertices.data(), uint32_t(vertices.size() * sizeof(Rml::Vertex))), m_layout);
			geometry.indices = bgfx::createIndexBuffer(bgfx::copy(indices.data(), uint32_t(indices.size() * sizeof(int))), BGFX_BUFFER_INDEX32);
			const Rml::CompiledGeometryHandle handle = m_next_geometry++;
			m_geometries.emplace(handle, geometry);
			return handle;
		}

		void RenderGeometry(Rml::CompiledGeometryHandle handle, Rml::Vector2f translation, Rml::TextureHandle texture) override
		{
			auto found = m_geometries.find(handle);
			if (found == m_geometries.end()) return;
			const float offset[4] = { translation.x, translation.y, 0.0f, 0.0f };
			bgfx::setUniform(m_u_offset, offset);
			if (m_has_transform) bgfx::setTransform(m_transform);
			if (m_scissor_on) bgfx::setScissor(m_scissor[0], m_scissor[1], m_scissor[2], m_scissor[3]);
			bgfx::TextureHandle tex = m_white;
			if (texture)
			{
				auto t = m_textures.find(texture);
				if (t != m_textures.end()) tex = t->second;
			}
			bgfx::setTexture(0, m_s_texture, tex);
			bgfx::setVertexBuffer(0, found->second.vertices);
			bgfx::setIndexBuffer(found->second.indices);
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA
				| BGFX_STATE_BLEND_FUNC_SEPARATE(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA, BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA));
			bgfx::submit(m_view, m_program);
		}

		void ReleaseGeometry(Rml::CompiledGeometryHandle handle) override
		{
			auto found = m_geometries.find(handle);
			if (found == m_geometries.end()) return;
			bgfx::destroy(found->second.vertices);
			bgfx::destroy(found->second.indices);
			m_geometries.erase(found);
		}

		Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) override
		{
			std::ifstream input(resolve(source), std::ios::binary);
			if (!input) return 0;
			std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
			bx::DefaultAllocator allocator;
			bimg::ImageContainer* image = bimg::imageParse(&allocator, bytes.data(), uint32_t(bytes.size()), bimg::TextureFormat::RGBA8);
			if (!image) return 0;
			// RmlUi blends with premultiplied alpha
			uint8_t* pixels = static_cast<uint8_t*>(image->m_data);
			const uint32_t count = image->m_width * image->m_height;
			for (uint32_t i = 0; i < count; ++i)
			{
				uint8_t* p = pixels + i * 4;
				const uint32_t a = p[3];
				p[0] = uint8_t(p[0] * a / 255); p[1] = uint8_t(p[1] * a / 255); p[2] = uint8_t(p[2] * a / 255);
			}
			dimensions = Rml::Vector2i(int(image->m_width), int(image->m_height));
			const bgfx::TextureHandle texture = bgfx::createTexture2D(uint16_t(image->m_width), uint16_t(image->m_height), false, 1,
				bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, bgfx::copy(pixels, count * 4));
			bimg::imageFree(image);
			return add_texture(texture);
		}

		Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions) override
		{
			const bgfx::TextureHandle texture = bgfx::createTexture2D(uint16_t(dimensions.x), uint16_t(dimensions.y), false, 1,
				bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, bgfx::copy(source.data(), uint32_t(source.size())));
			return add_texture(texture);
		}

		void ReleaseTexture(Rml::TextureHandle handle) override
		{
			auto found = m_textures.find(handle);
			if (found == m_textures.end()) return;
			bgfx::destroy(found->second);
			m_textures.erase(found);
		}

		void EnableScissorRegion(bool enable) override { m_scissor_on = enable; }

		void SetScissorRegion(Rml::Rectanglei region) override
		{
			const int x = std::max(0, region.Left()), y = std::max(0, region.Top());
			m_scissor[0] = uint16_t(x);
			m_scissor[1] = uint16_t(y);
			m_scissor[2] = uint16_t(std::max(0, std::min(int(m_width), region.Right()) - x));
			m_scissor[3] = uint16_t(std::max(0, std::min(int(m_height), region.Bottom()) - y));
		}

		void SetTransform(const Rml::Matrix4f* transform) override
		{
			m_has_transform = transform != nullptr;
			// RmlUi keeps its matrices by columns, as bgfx expects them
			if (transform) std::memcpy(m_transform, transform->data(), sizeof(m_transform));
		}

		// --- SystemInterface ---
		double GetElapsedTime() override
		{
			return std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count();
		}

		bool LogMessage(Rml::Log::Type type, const Rml::String& message) override
		{
			if (type == Rml::Log::LT_ERROR || type == Rml::Log::LT_ASSERT) { LOG_ERROR("[UI] %s", message.c_str()); }
			else if (type == Rml::Log::LT_WARNING) { LOG_WARNING("[UI] %s", message.c_str()); }
			else { LOG_INFO("[UI] %s", message.c_str()); }
			return true;
		}

		// --- FileInterface ---
		Rml::FileHandle Open(const Rml::String& path) override
		{
			return reinterpret_cast<Rml::FileHandle>(std::fopen(resolve(path).c_str(), "rb"));
		}
		void Close(Rml::FileHandle file) override { std::fclose(reinterpret_cast<FILE*>(file)); }
		size_t Read(void* buffer, size_t size, Rml::FileHandle file) override { return std::fread(buffer, 1, size, reinterpret_cast<FILE*>(file)); }
		bool Seek(Rml::FileHandle file, long offset, int origin) override { return std::fseek(reinterpret_cast<FILE*>(file), offset, origin) == 0; }
		size_t Tell(Rml::FileHandle file) override { return size_t(std::ftell(reinterpret_cast<FILE*>(file))); }

	private:
		Rml::TextureHandle add_texture(bgfx::TextureHandle texture)
		{
			if (!bgfx::isValid(texture)) return 0;
			const Rml::TextureHandle handle = m_next_texture++;
			m_textures.emplace(handle, texture);
			return handle;
		}

		struct Geometry { bgfx::VertexBufferHandle vertices; bgfx::IndexBufferHandle indices; };
		BgfxGraphicsSystem* m_graphics;
		bgfx::ViewId m_view = 0;
		bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
		bgfx::VertexLayout m_layout;
		bgfx::UniformHandle m_u_offset = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_s_texture = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle m_white = BGFX_INVALID_HANDLE;
		std::map<Rml::CompiledGeometryHandle, Geometry> m_geometries;
		std::map<Rml::TextureHandle, bgfx::TextureHandle> m_textures;
		Rml::CompiledGeometryHandle m_next_geometry = 1;
		Rml::TextureHandle m_next_texture = 1;
		unsigned m_width = 0, m_height = 0;
		bool m_scissor_on = false;
		uint16_t m_scissor[4] = {};
		bool m_has_transform = false;
		float m_transform[16] = {};
		std::chrono::steady_clock::time_point m_start;
	};

	// A click in a document: the id of the clicked element or of its nearest parent with one
	class BgfxRmlUi::ClickListener final : public Rml::EventListener
	{
	public:
		explicit ClickListener(ClickCallback callback) : m_callback(std::move(callback)) {}
		void ProcessEvent(Rml::Event& event) override
		{
			for (Rml::Element* element = event.GetTargetElement(); element; element = element->GetParentNode())
			{
				if (!element->GetId().empty())
				{
					m_callback(element->GetId());
					return;
				}
			}
		}
	private:
		ClickCallback m_callback;
	};

	// ---------------------------------------------------------------------------------------------------------------

	BgfxRmlUi::BgfxRmlUi(BgfxGraphicsSystem* graphics) : m_graphics(graphics)
	{
	}

	BgfxRmlUi::~BgfxRmlUi()
	{
		terminate();
	}

	void BgfxRmlUi::initialize()
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (m_initialized || !m_graphics) return;
		m_backend = std::make_unique<Backend>(m_graphics);
		if (!m_backend->create(m_graphics->get_ui_view_id()))
		{
			LOG_ERROR("[UI] The interface's shaders (vs_ui/fs_ui) did not load: no game interface");
			m_backend.reset();
			return;
		}
		Rml::SetRenderInterface(m_backend.get());
		Rml::SetSystemInterface(m_backend.get());
		Rml::SetFileInterface(m_backend.get());
		if (!Rml::Initialise())
		{
			LOG_ERROR("[UI] RmlUi did not start");
			return;
		}
		m_width = m_graphics->get_output_width();
		m_height = m_graphics->get_output_height();
		m_context = Rml::CreateContext("main", Rml::Vector2i(int(m_width), int(m_height)));
		m_context->SetDensityIndependentPixelRatio(float(m_height) / 720.0f);
		attach_input(OSMgr.get_input_manager());
		m_initialized = true;
		LOG_INFO("[UI] RmlUi %s ready (%ux%u)", Rml::GetVersion().c_str(), m_width, m_height);
	}

	void BgfxRmlUi::terminate()
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (!m_initialized) return;
		detach_input();
		m_listeners.clear();
		m_documents.clear();
		Rml::Shutdown();   // releases the documents, then their geometry and textures through the backend
		m_context = nullptr;
		m_backend->destroy();
		m_backend.reset();
		m_initialized = false;
	}

	bool BgfxRmlUi::load_font(const Path& resourcePath)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (!m_initialized) return false;
		const bool loaded = Rml::LoadFontFace(resourcePath.c_str());
		if (!loaded) LOG_WARNING("[UI] font %s did not load", resourcePath.c_str());
		return loaded;
	}

	void BgfxRmlUi::set_theme(const Path& themePath)
	{
		// The look lives in each document's style sheets (<link type="text/rcss">)
		(void)themePath;
	}

	bool BgfxRmlUi::load_document(unsigned id, const Path& resourcePath)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (!m_initialized) return false;
		unload_document(id);
		Rml::ElementDocument* document = m_context->LoadDocument(resourcePath.c_str());
		if (!document)
		{
			LOG_ERROR("[UI] document %s did not load", resourcePath.c_str());
			return false;
		}
		m_documents[id] = document;
		return true;
	}

	bool BgfxRmlUi::unload_document(unsigned id)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		auto found = m_documents.find(id);
		if (found == m_documents.end()) return false;
		for (auto it = m_listeners.begin(); it != m_listeners.end();)
		{
			if (it->second.document == id)
			{
				found->second->RemoveEventListener(Rml::EventId::Click, it->second.listener.get());
				it = m_listeners.erase(it);
			}
			else ++it;
		}
		found->second->Close();
		m_documents.erase(found);
		return true;
	}

	TaskPtr<bool> BgfxRmlUi::load_document_async(unsigned id, const Path& resourcePath)
	{
		return std::make_shared<Task<bool>>(load_document(id, resourcePath));
	}

	TaskPtr<bool> BgfxRmlUi::unload_document_async(unsigned id)
	{
		return std::make_shared<Task<bool>>(unload_document(id));
	}

	void BgfxRmlUi::show_document(unsigned id, bool autoresize)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		(void)autoresize;
		auto found = m_documents.find(id);
		if (found != m_documents.end()) found->second->Show();
	}

	void BgfxRmlUi::hide_document(unsigned id)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		auto found = m_documents.find(id);
		if (found != m_documents.end()) found->second->Hide();
	}

	bool BgfxRmlUi::is_document_visible(unsigned id) const
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		auto found = m_documents.find(id);
		return found != m_documents.end() && found->second->IsVisible();
	}

	Rml::Element* BgfxRmlUi::find(unsigned document, const std::string& element) const
	{
		auto found = m_documents.find(document);
		return found == m_documents.end() ? nullptr : found->second->GetElementById(element);
	}

	void BgfxRmlUi::set_text(unsigned document, const std::string& element, const std::string& text)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (Rml::Element* e = find(document, element))
		{
			Rml::String escaped;
			escaped.reserve(text.size());
			for (char c : text)
			{
				if (c == '<') escaped += "&lt;";
				else if (c == '>') escaped += "&gt;";
				else if (c == '&') escaped += "&amp;";
				else escaped += c;
			}
			// Only when it changes: setting the content rebuilds the element
			if (e->GetInnerRML() != escaped) e->SetInnerRML(escaped);
		}
	}

	void BgfxRmlUi::set_markup(unsigned document, const std::string& element, const std::string& markup)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (Rml::Element* e = find(document, element)) e->SetInnerRML(markup);
	}

	void BgfxRmlUi::set_class(unsigned document, const std::string& element, const std::string& name, bool on)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (Rml::Element* e = find(document, element))
		{
			if (e->IsClassSet(name) != on) e->SetClass(name, on);
		}
	}

	void BgfxRmlUi::set_property(unsigned document, const std::string& element, const std::string& name, const std::string& value)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (Rml::Element* e = find(document, element)) e->SetProperty(name, value);
	}

	void BgfxRmlUi::set_attribute(unsigned document, const std::string& element, const std::string& name, const std::string& value)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (Rml::Element* e = find(document, element)) e->SetAttribute(name, value);
	}

	unsigned BgfxRmlUi::add_click_listener(unsigned document, ClickCallback callback)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		auto found = m_documents.find(document);
		if (found == m_documents.end()) return 0;
		auto listener = std::make_unique<ClickListener>(std::move(callback));
		found->second->AddEventListener(Rml::EventId::Click, listener.get());
		const unsigned id = m_next_listener++;
		m_listeners[id] = Listener{ document, std::move(listener) };
		return id;
	}

	void BgfxRmlUi::remove_click_listener(unsigned listener)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		auto found = m_listeners.find(listener);
		if (found == m_listeners.end()) return;
		auto document = m_documents.find(found->second.document);
		if (document != m_documents.end()) document->second->RemoveEventListener(Rml::EventId::Click, found->second.listener.get());
		m_listeners.erase(found);
	}

	bool BgfxRmlUi::is_over_ui(int x, int y) const
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (!m_context) return false;
		// The documents' bodies cover the screen; only what is inside them (with pointer-events) takes input
		Rml::Element* element = m_context->GetElementAtPoint(Rml::Vector2f(float(x), float(y)));
		return element && element->GetTagName() != "body" && element != m_context->GetRootElement();
	}

	void BgfxRmlUi::pointer(int x, int y, bool down)
	{
		InputEvent move{ InputEvent::Type::Move }; move.x = x; move.y = y; push(move);
		InputEvent button{ InputEvent::Type::Button }; button.x = x; button.y = y; button.down = down; push(button);
	}

	void BgfxRmlUi::pointer_move(int x, int y)
	{
		InputEvent move{ InputEvent::Type::Move }; move.x = x; move.y = y; push(move);
	}

	void BgfxRmlUi::touch(int id, int x, int y, int phase)
	{
		InputEvent e{ InputEvent::Type::Touch }; e.x = x; e.y = y; e.touch = id; e.phase = phase; push(e);
	}

	void BgfxRmlUi::attach_input(IInputManager* input)
	{
		detach_input();
		m_input = input;
		if (!input) return;
		m_ids[0] = input->event_mouse_moved.add_listener([this](const MouseArgs& a)
		{
			InputEvent e{ InputEvent::Type::Move }; e.x = int(a.x); e.y = int(a.y); push(e);
		});
		m_ids[1] = input->event_mouse_pressed.add_listener([this](const MouseArgs& a)
		{
			InputEvent e{ InputEvent::Type::Button }; e.x = int(a.x); e.y = int(a.y); e.button = int(a.button_id); e.down = true; push(e);
		});
		m_ids[2] = input->event_mouse_released.add_listener([this](const MouseArgs& a)
		{
			InputEvent e{ InputEvent::Type::Button }; e.x = int(a.x); e.y = int(a.y); e.button = int(a.button_id); e.down = false; push(e);
		});
		m_ids[3] = input->event_mouse_wheel.add_listener([this](const MouseArgs& a)
		{
			InputEvent e{ InputEvent::Type::Wheel }; e.wheel = a.delta; push(e);
		});
		m_ids[4] = input->event_key_down.add_listener([this](const KeyArgs& a)
		{
			InputEvent e{ InputEvent::Type::Key }; e.key = a.key; e.modifiers = a.key_modifier; e.down = true; push(e);
		});
		m_ids[5] = input->event_key_up.add_listener([this](const KeyArgs& a)
		{
			InputEvent e{ InputEvent::Type::Key }; e.key = a.key; e.modifiers = a.key_modifier; e.down = false; push(e);
		});
		m_ids[6] = input->event_text.add_listener([this](const TextArgs& a)
		{
			InputEvent e{ InputEvent::Type::Text }; e.text = a.unicode; push(e);
		});
	}

	void BgfxRmlUi::detach_input()
	{
		if (!m_input) return;
		m_input->event_mouse_moved.remove_listener(m_ids[0]);
		m_input->event_mouse_pressed.remove_listener(m_ids[1]);
		m_input->event_mouse_released.remove_listener(m_ids[2]);
		m_input->event_mouse_wheel.remove_listener(m_ids[3]);
		m_input->event_key_down.remove_listener(m_ids[4]);
		m_input->event_key_up.remove_listener(m_ids[5]);
		m_input->event_text.remove_listener(m_ids[6]);
		m_input = nullptr;
	}

	void BgfxRmlUi::push(const InputEvent& event)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_events.push_back(event);
	}

	void BgfxRmlUi::feed_events()
	{
		std::vector<InputEvent> events;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			events.swap(m_events);
		}
		// What the development panels take does not reach the game's interface
		const bool debugMouse = m_graphics->get_debug_ui().wants_mouse();
		const bool debugKeys = m_graphics->get_debug_ui().wants_keyboard();
		for (const InputEvent& e : events)
		{
			switch (e.type)
			{
			case InputEvent::Type::Move: m_context->ProcessMouseMove(e.x, e.y, 0); break;
			case InputEvent::Type::Button:
				if (debugMouse && e.down) break;
				m_context->ProcessMouseMove(e.x, e.y, 0);
				if (e.down) m_context->ProcessMouseButtonDown(e.button, 0);
				else m_context->ProcessMouseButtonUp(e.button, 0);
				break;
			case InputEvent::Type::Touch:
			{
				// RmlUi's touch input: a tap clicks, a drag scrolls the list under the finger (with inertia)
				const Rml::TouchList touches{ Rml::Touch{ Rml::TouchId(e.touch), Rml::Vector2f(float(e.x), float(e.y)) } };
				if (e.phase == 0) m_context->ProcessTouchStart(touches, 0);
				else if (e.phase == 1) m_context->ProcessTouchMove(touches, 0);
				else m_context->ProcessTouchEnd(touches, 0);
				break;
			}
			case InputEvent::Type::Wheel: if (!debugMouse) m_context->ProcessMouseWheel(Rml::Vector2f(0.0f, -e.wheel), 0); break;
			case InputEvent::Type::Key:
				if (debugKeys) break;
				if (e.down) m_context->ProcessKeyDown(to_rml(e.key), to_rml_modifiers(e.modifiers));
				else m_context->ProcessKeyUp(to_rml(e.key), to_rml_modifiers(e.modifiers));
				break;
			case InputEvent::Type::Text:
				if (!debugKeys && e.text >= 32 && e.text != 127) m_context->ProcessTextInput(Rml::Character(e.text));
				break;
			}
		}
	}

	void BgfxRmlUi::draw_ui()
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (!m_initialized) return;
		const unsigned width = m_graphics->get_output_width(), height = m_graphics->get_output_height();
		if (width != m_width || height != m_height)
		{
			m_width = width;
			m_height = height;
			m_context->SetDimensions(Rml::Vector2i(int(width), int(height)));
			m_context->SetDensityIndependentPixelRatio(float(height) / 720.0f);
		}
		feed_events();
		m_context->Update();
		m_backend->begin_frame(width, height);
		m_context->Render();
	}
}
