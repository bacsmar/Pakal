#include "BgfxRmlUi.h"

#include "BgfxGraphicsSystem.h"
#include "FontOutline.h"
#include "IInputManager.h"
#include "LogMgr.h"
#include "OSManager.h"
#include "ResourceManager.h"
#include "Task.h"

#include <RmlUi/Core.h>
#include <nlohmann/json.hpp>
#include <bimg/decode.h>
#include <bx/allocator.h>
#include <bx/math.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <set>

namespace Pakal
{
	namespace
	{
		// font-effect: outline, as RmlUi's (its properties, its picture) but made in time that does not grow with its width
		// (FontOutline.h): RmlUi's own visits a disc of (2w+1)^2 pixels for each pixel, and a title's letters with a thick
		// outline (an announcer's, 200 dp) took a phone's CPU a second each, the first time they were shown
		class FastOutline final : public Rml::FontEffect
		{
		public:
			explicit FastOutline(int width) : m_width(width) { SetLayer(Layer::Back); }

			bool HasUniqueTexture() const override { return true; }

			bool GetGlyphMetrics(Rml::Vector2i& origin, Rml::Vector2i& dimensions, const Rml::FontGlyph&) const override
			{
				if (dimensions.x * dimensions.y <= 0) return false;
				origin.x -= m_width;
				origin.y -= m_width;
				dimensions.x += 2 * m_width;
				dimensions.y += 2 * m_width;
				return true;
			}

			void GenerateGlyphTexture(Rml::byte* destination, const Rml::Vector2i size, int stride, const Rml::FontGlyph& glyph) const override
			{
				if (!glyph.bitmap_data || size.x < glyph.bitmap_dimensions.x + 2 * m_width || size.y < glyph.bitmap_dimensions.y + 2 * m_width) return;
				const bool colour = glyph.color_format == Rml::ColorFormat::RGBA8;
				const int bytes = colour ? 4 : 1;
				outline_glyph(glyph.bitmap_data, glyph.bitmap_dimensions.x, glyph.bitmap_dimensions.y, glyph.bitmap_dimensions.x * bytes, bytes, colour ? 3 : 0,
					destination, stride, 4, 3, m_width);
				FillColorValuesFromAlpha(destination, size, stride);
			}

		private:
			int m_width;
		};

		class FastOutlineInstancer final : public Rml::FontEffectInstancer
		{
		public:
			FastOutlineInstancer()
			{
				m_width = RegisterProperty("width", "1px", true).AddParser("length").GetId();
				m_color = RegisterProperty("color", "white", false).AddParser("color").GetId();
				RegisterShorthand("font-effect", "width, color", Rml::ShorthandType::FallThrough);
			}

			Rml::SharedPtr<Rml::FontEffect> InstanceFontEffect(const Rml::String&, const Rml::PropertyDictionary& properties) override
			{
				const int width = int(properties.GetProperty(m_width)->Get<float>());
				if (width <= 0) return nullptr;
				auto effect = Rml::MakeShared<FastOutline>(width);
				effect->SetColour(properties.GetProperty(m_color)->Get<Rml::Colourb>());
				return effect;
			}

		private:
			Rml::PropertyId m_width = Rml::PropertyId::Invalid, m_color = Rml::PropertyId::Invalid;
		};

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
		std::mutex m_stringsLock;
		std::map<std::string, std::string> m_strings;
		std::vector<std::string>* m_opened = nullptr;   // while a document loads to be watched: the files it reads

		explicit Backend(BgfxGraphicsSystem* graphics) : m_graphics(graphics)
		{
			m_start = std::chrono::steady_clock::now();
		}

		bool create(bgfx::ViewId view)
		{
			m_view = view;
			// OpenGL resolves user uniforms when the program is linked. Register them first.
			m_u_offset = bgfx::createUniform("u_uiOffset", bgfx::UniformType::Vec4);
			m_s_texture = bgfx::createUniform("s_uiTexture", bgfx::UniformType::Sampler);
			m_program = m_graphics->load_program("vs_ui", "fs_ui");
			if (!bgfx::isValid(m_program))
			{
				bgfx::destroy(m_u_offset);
				bgfx::destroy(m_s_texture);
				m_u_offset = m_s_texture = BGFX_INVALID_HANDLE;
				return false;
			}
			m_layout.begin()
				.add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
				.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
				.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
				.end();
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
			const auto bytes = ResourceMgr.read_bytes(source);
			if (!bytes || bytes->empty()) return 0;
			bx::DefaultAllocator allocator;
			bimg::ImageContainer* image = bimg::imageParse(&allocator, bytes->data(), uint32_t(bytes->size()), bimg::TextureFormat::RGBA8);
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
		// The documents' texts by key (set_strings): a text that is a key shows the table's
		int TranslateString(Rml::String& translated, const Rml::String& input) override
		{
			std::lock_guard<std::mutex> lock(m_stringsLock);
			auto found = m_strings.find(input);
			if (found == m_strings.end())
			{
				translated = input;
				return 0;
			}
			translated = found->second;
			return 1;
		}
		void set_strings(const std::map<std::string, std::string>& strings)
		{
			std::lock_guard<std::mutex> lock(m_stringsLock);
			m_strings = strings;
		}

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

		// --- FileInterface: documents, style sheets and fonts through the resource manager (a folder, a zip, the APK) ---
		struct OpenFile
		{
			SharedPtr<const std::vector<uint8_t>> bytes;
			size_t at = 0;
		};
		Rml::FileHandle Open(const Rml::String& path) override
		{
			auto bytes = ResourceMgr.read_bytes(path);
			if (bytes && m_opened) m_opened->push_back(path);
			return bytes ? reinterpret_cast<Rml::FileHandle>(new OpenFile{ std::move(bytes), 0 }) : 0;
		}
		void Close(Rml::FileHandle file) override { delete reinterpret_cast<OpenFile*>(file); }
		size_t Read(void* buffer, size_t size, Rml::FileHandle file) override
		{
			OpenFile& open = *reinterpret_cast<OpenFile*>(file);
			const size_t count = std::min(size, open.bytes->size() - open.at);
			std::memcpy(buffer, open.bytes->data() + open.at, count);
			open.at += count;
			return count;
		}
		bool Seek(Rml::FileHandle file, long offset, int origin) override
		{
			OpenFile& open = *reinterpret_cast<OpenFile*>(file);
			const long base = origin == SEEK_SET ? 0 : origin == SEEK_CUR ? long(open.at) : long(open.bytes->size());
			const long to = base + offset;
			if (to < 0 || size_t(to) > open.bytes->size()) return false;
			open.at = size_t(to);
			return true;
		}
		size_t Tell(Rml::FileHandle file) override { return reinterpret_cast<OpenFile*>(file)->at; }
		size_t Length(Rml::FileHandle file) override { return reinterpret_cast<OpenFile*>(file)->bytes->size(); }

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
	// ---------------------------------------------------------------------------------------------------------------
	// Data models: RmlUi's variables over a JSON tree. A node is a scalar, a list or a record; each kind has one definition
	// that reads the node it is given (its children are found as they are asked for), so any tree binds without types
	// ---------------------------------------------------------------------------------------------------------------
	namespace
	{
		// The interfaces are made for a 1280x720 screen: a dp is a pixel of it. The design fits whole on any screen, by its height on a
		// screen as wide as 16:9 or wider, by its width on a narrower one (a tablet, an open folding phone): there the 720 dp of the
		// height are more, and nothing that was laid out across 1280 dp falls off the sides or over its neighbours
		// A TrueType or OpenType font's family name (its 'name' table, name 1), read through RmlUi's files; empty when unknown
		std::string font_family(const std::string& path)
		{
			Rml::FileInterface* files = Rml::GetFileInterface();
			if (!files) return {};
			const Rml::FileHandle file = files->Open(path);
			if (!file) return {};
			std::vector<unsigned char> data(files->Length(file));
			const size_t read = data.empty() ? 0 : files->Read(data.data(), data.size(), file);
			files->Close(file);
			data.resize(read);
			auto u16 = [&data](size_t at) -> unsigned { return at + 2 <= data.size() ? unsigned(data[at] << 8 | data[at + 1]) : 0u; };
			auto u32 = [&data, &u16](size_t at) -> size_t { return (size_t(u16(at)) << 16) | u16(at + 2); };
			const unsigned tables = u16(4);
			for (unsigned t = 0; t < tables; ++t)
			{
				const size_t entry = 12 + size_t(t) * 16;
				if (entry + 16 > data.size() || std::memcmp(&data[entry], "name", 4) != 0) continue;
				const size_t table = u32(entry + 8);
				const unsigned count = u16(table + 2);
				const size_t strings = table + u16(table + 4);
				std::string mac;
				for (unsigned r = 0; r < count; ++r)
				{
					const size_t record = table + 6 + size_t(r) * 12;
					const unsigned platform = u16(record), nameId = u16(record + 6);
					const size_t length = u16(record + 8), offset = strings + u16(record + 10);
					if (nameId != 1 || offset + length > data.size()) continue;
					if (platform == 3 || platform == 0)   // UTF-16 big endian: the family names are ASCII in practice
					{
						std::string name;
						for (size_t i = 0; i + 1 < length; i += 2)
						{
							const unsigned c = u16(offset + i);
							name += c < 128 ? char(c) : '?';
						}
						if (!name.empty()) return name;
					}
					else if (platform == 1 && mac.empty())
						mac.assign(reinterpret_cast<const char*>(&data[offset]), length);
				}
				return mac;
			}
			return {};
		}

		float design_ratio(unsigned width, unsigned height)
		{
			return std::min(float(height) / 720.0f, float(width) / 1280.0f);
		}

		using Json = nlohmann::json;
		Rml::VariableDefinition* definition_of(const Json& node);

		class JsonScalar final : public Rml::VariableDefinition
		{
		public:
			JsonScalar() : Rml::VariableDefinition(Rml::DataVariableType::Scalar) {}
			bool Get(void* ptr, Rml::Variant& variant) override
			{
				const Json& node = *static_cast<const Json*>(ptr);
				if (node.is_boolean()) variant = node.get<bool>();
				else if (node.is_number_integer()) variant = node.get<int>();
				else if (node.is_number()) variant = node.get<double>();
				else if (node.is_string()) variant = node.get<std::string>();
				else variant = Rml::String();
				return true;
			}
		};

		class JsonList final : public Rml::VariableDefinition
		{
		public:
			JsonList() : Rml::VariableDefinition(Rml::DataVariableType::Array) {}
			int Size(void* ptr) override { return static_cast<int>(static_cast<Json*>(ptr)->size()); }
			Rml::DataVariable Child(void* ptr, const Rml::DataAddressEntry& address) override
			{
				Json& node = *static_cast<Json*>(ptr);
				if (address.index < 0 || address.index >= static_cast<int>(node.size()))
				{
					if (address.name == "size") return Rml::MakeLiteralIntVariable(static_cast<int>(node.size()));
					return Rml::DataVariable();
				}
				Json& child = node[static_cast<size_t>(address.index)];
				return Rml::DataVariable(definition_of(child), &child);
			}
		};

		class JsonRecord final : public Rml::VariableDefinition
		{
		public:
			JsonRecord() : Rml::VariableDefinition(Rml::DataVariableType::Struct) {}
			Rml::DataVariable Child(void* ptr, const Rml::DataAddressEntry& address) override
			{
				Json& node = *static_cast<Json*>(ptr);
				auto found = node.find(address.name);
				if (found == node.end()) return Rml::DataVariable();
				return Rml::DataVariable(definition_of(*found), &*found);
			}
			Rml::StringList ReflectMemberNames() override { return {}; }
		};

		Rml::VariableDefinition* definition_of(const Json& node)
		{
			static JsonScalar scalar;
			static JsonList list;
			static JsonRecord record;
			if (node.is_array()) return &list;
			if (node.is_object()) return &record;
			return &scalar;
		}
	}

	struct BgfxRmlUi::DataModel
	{
		Rml::DataModelConstructor constructor;
		Rml::DataModelHandle handle;
		Json root;   // its variables: the members of this object (bound by address: they are replaced, never moved)
	};

	bool BgfxRmlUi::create_data_model(const std::string& model, const std::string& json)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (!m_initialized || m_models.count(model)) return false;
		auto data = std::make_unique<DataModel>();
		data->root = Json::parse(json, nullptr, false);
		if (!data->root.is_object())
		{
			LOG_ERROR("[UI] data model %s: its data is not a JSON object", model.c_str());
			return false;
		}
		data->constructor = m_context->CreateDataModel(model, nullptr, true);
		if (!data->constructor) return false;
		for (auto it = data->root.begin(); it != data->root.end(); ++it)
		{
			data->constructor.BindCustomDataVariable(it.key(), Rml::DataVariable(definition_of(*it), &*it));
		}
		data->handle = data->constructor.GetModelHandle();
		m_models[model] = std::move(data);
		return true;
	}

	void BgfxRmlUi::remove_data_model(const std::string& model)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		auto found = m_models.find(model);
		if (found == m_models.end()) return;
		if (m_context) m_context->RemoveDataModel(model);
		m_models.erase(found);
	}

	void BgfxRmlUi::set_data(const std::string& model, const std::string& variable, const std::string& json)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		auto found = m_models.find(model);
		if (found == m_models.end()) return;
		DataModel& data = *found->second;
		auto member = data.root.find(variable);
		if (member == data.root.end())
		{
			LOG_WARNING("[UI] data model %s has no variable %s (variables are made with the model)", model.c_str(), variable.c_str());
			return;
		}
		Json value = Json::parse(json, nullptr, false);
		if (value.is_discarded()) return;
		if (definition_of(value) != definition_of(*member))
		{
			LOG_WARNING("[UI] data model %s: %s keeps its kind (a list stays a list, a record a record)", model.c_str(), variable.c_str());
			return;
		}
		if (*member == value) return;
		*member = std::move(value);   // in place: the binding holds this node
		data.handle.DirtyVariable(variable);
	}

	void BgfxRmlUi::add_data_event(const std::string& model, const std::string& name, DataEventCallback callback)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		auto found = m_models.find(model);
		if (found == m_models.end() || !callback) return;
		found->second->constructor.BindEventCallback(name, [callback](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& arguments)
		{
			std::vector<std::string> texts;
			for (const Rml::Variant& argument : arguments) texts.push_back(argument.Get<Rml::String>());
			callback(texts);
		});
	}

	void BgfxRmlUi::set_strings(const std::map<std::string, std::string>& strings)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (m_backend) m_backend->set_strings(strings);
	}

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
		{
			static FastOutlineInstancer outline;   // (in place of RmlUi's own: the same font-effect, made faster)
			Rml::Factory::RegisterFontEffectInstancer("outline", &outline);
		}
		m_width = m_graphics->get_output_width();
		m_height = m_graphics->get_output_height();
		m_context = Rml::CreateContext("main", Rml::Vector2i(int(m_width), int(m_height)));
		m_context->SetDensityIndependentPixelRatio(design_ratio(m_width, m_height));
		attach_input(OSMgr.get_input_manager());
		m_reload = ResourceMgr.reload_enabled();
		m_initialized = true;
		LOG_INFO("[UI] RmlUi %s ready (%ux%u)", Rml::GetVersion().c_str(), m_width, m_height);
	}

	void BgfxRmlUi::terminate()
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (!m_initialized) return;
		detach_input();
		m_listeners.clear();
		while (!m_sources.empty()) forget_source(m_sources.begin()->first);
		m_documents.clear();
		for (const auto& model : m_models) m_context->RemoveDataModel(model.first);
		m_models.clear();
		m_label_document = nullptr;
		m_label_font.clear();
		m_label_dirty = true;
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
		if (loaded && m_label_font.empty())
		{
			m_label_font = font_family(resourcePath.c_str());
			m_label_dirty = true;
		}
		return loaded;
	}

	bool BgfxRmlUi::load_fallback_font(const Path& resourcePath)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (!m_initialized) return false;
		const bool loaded = Rml::LoadFontFace(resourcePath.c_str(), true);
		if (!loaded) LOG_WARNING("[UI] fallback font %s did not load", resourcePath.c_str());
		return loaded;
	}

	void BgfxRmlUi::set_theme(const Path& themePath)
	{
		// The look lives in each document's style sheets (<link type="text/rcss">)
		(void)themePath;
	}

	bool BgfxRmlUi::load_document_from_text(unsigned id, const std::string& markup)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (!m_initialized) return false;
		unload_document(id);
		Rml::ElementDocument* document = m_context->LoadDocumentFromMemory(markup, "[engine document]");
		if (!document)
		{
			LOG_ERROR("[UI] a document from its markup did not load (%u)", id);
			return false;
		}
		m_documents[id] = document;
		return true;
	}

	bool BgfxRmlUi::load_document(unsigned id, const Path& resourcePath)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (!m_initialized) return false;
		unload_document(id);
		Rml::ElementDocument* document = load_from(id, resourcePath.c_str());
		if (!document)
		{
			LOG_ERROR("[UI] document %s did not load", resourcePath.c_str());
			return false;
		}
		m_documents[id] = document;
		return true;
	}

	// The document read from its file; when files reload, watched with every file it read as it loaded (the style sheets
	// and templates it links: RmlUi's caches of them are emptied first, so that it reads them now and the new ones later)
	Rml::ElementDocument* BgfxRmlUi::load_from(unsigned id, const std::string& path)
	{
		if (!m_reload) return m_context->LoadDocument(path);
		Rml::Factory::ClearStyleSheetCache();
		Rml::Factory::ClearTemplateCache();
		std::vector<std::string> opened{ path };
		m_backend->m_opened = &opened;
		Rml::ElementDocument* document = m_context->LoadDocument(path);
		m_backend->m_opened = nullptr;
		if (!document) return nullptr;
		Source& source = m_sources[id];
		for (unsigned watch : source.watches) ResourceMgr.unwatch(watch);
		source.watches.clear();
		source.path = path;
		std::set<std::string> files;
		for (const std::string& file : opened) files.insert(ResourceMgr.logical_path(file));
		for (const std::string& file : files)
		{
			// (the callback runs on the engine's thread, where the game loads and changes its documents)
			const unsigned watch = ResourceMgr.watch(file, [this, id](const std::string&) { reload_document(id); });
			if (watch) source.watches.push_back(watch);
		}
		return document;
	}

	void BgfxRmlUi::forget_source(unsigned document)
	{
		auto found = m_sources.find(document);
		if (found == m_sources.end()) return;
		for (unsigned watch : found->second.watches) ResourceMgr.unwatch(watch);
		m_sources.erase(found);
	}

	void BgfxRmlUi::remember(unsigned document, Applied applied)
	{
		if (!m_reload || !m_documents.count(document)) return;
		// put back in this order: the contents (they may make the elements the rest names), attributes, properties, classes
		static const char* order[] = { "0", "0", "1", "2", "3" };
		std::string key = std::string(order[int(applied.kind)]) + applied.element;
		if (applied.kind != Applied::Kind::Text && applied.kind != Applied::Kind::Markup) key += '\n' + applied.name;
		m_sources[document].applied[key] = std::move(applied);
	}

	bool BgfxRmlUi::reload_document(unsigned id)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (!m_initialized) return false;
		auto found = m_documents.find(id);
		auto source = m_sources.find(id);
		if (found == m_documents.end() || source == m_sources.end() || source->second.path.empty()) return false;
		Rml::ElementDocument* old = found->second;
		const bool visible = old->IsVisible();
		std::string focus;
		if (Rml::Element* focused = m_context->GetFocusElement(); focused && focused->GetOwnerDocument() == old) focus = focused->GetId();
		std::vector<Rml::ElementDocument*> order;   // the documents back to front as they were, to stack them so again
		for (int i = 0; i < m_context->GetNumDocuments(); ++i) order.push_back(m_context->GetDocument(i));
		const std::string path = source->second.path;
		Rml::ElementDocument* fresh = load_from(id, path);
		if (!fresh)
		{
			LOG_ERROR("[UI] document %s did not reload: the one loaded stays", path.c_str());
			return false;
		}
		for (auto& [listener, entry] : m_listeners)
		{
			if (entry.document != id) continue;
			old->RemoveEventListener(Rml::EventId::Click, entry.listener.get());
			fresh->AddEventListener(Rml::EventId::Click, entry.listener.get());
		}
		old->Close();
		found->second = fresh;
		for (const auto& [key, applied] : m_sources[id].applied)
		{
			Rml::Element* e = applied.element.empty() ? nullptr : fresh->GetElementById(applied.element);
			if (!e) continue;
			switch (applied.kind)
			{
			case Applied::Kind::Text:
			case Applied::Kind::Markup: e->SetInnerRML(applied.value); break;
			case Applied::Kind::Attribute: e->SetAttribute(applied.name, applied.value); break;
			case Applied::Kind::Property: e->SetProperty(applied.name, applied.value); break;
			case Applied::Kind::Class: e->SetClass(applied.name, applied.on); break;
			}
		}
		if (visible) fresh->Show(Rml::ModalFlag::None, Rml::FocusFlag::None, Rml::ScrollFlag::None);
		std::set<Rml::ElementDocument*> ours;
		for (const auto& [document, element] : m_documents) ours.insert(element);
		for (Rml::ElementDocument* document : order)
		{
			if (document == old) document = fresh;
			if (ours.count(document)) document->PullToFront();
		}
		if (!focus.empty())
		{
			if (Rml::Element* e = fresh->GetElementById(focus)) e->Focus();
		}
		LOG_INFO("[UI] document %u reloaded from %s", id, path.c_str());
		return true;
	}

	void BgfxRmlUi::reload_documents()
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		std::vector<unsigned> ids;
		for (const auto& [id, document] : m_documents) ids.push_back(id);
		for (unsigned id : ids) reload_document(id);
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
		forget_source(id);
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

	void BgfxRmlUi::bring_to_front(unsigned id)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		auto found = m_documents.find(id);
		if (found != m_documents.end()) found->second->PullToFront();
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

	std::vector<IUIManager::DocumentInfo> BgfxRmlUi::documents() const
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		std::vector<DocumentInfo> out;
		for (const auto& [id, document] : m_documents)
		{
			DocumentInfo info;
			info.id = id;
			info.source = document->GetSourceURL();
			info.title = document->GetTitle();
			info.visible = document->IsVisible();
			out.push_back(std::move(info));
		}
		return out;
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
			remember(document, Applied{ Applied::Kind::Text, element, std::string(), escaped });
		}
	}

	void BgfxRmlUi::set_markup(unsigned document, const std::string& element, const std::string& markup)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (Rml::Element* e = find(document, element))
		{
			e->SetInnerRML(markup);
			remember(document, Applied{ Applied::Kind::Markup, element, std::string(), markup });
		}
	}

	void BgfxRmlUi::set_class(unsigned document, const std::string& element, const std::string& name, bool on)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (Rml::Element* e = find(document, element))
		{
			if (e->IsClassSet(name) != on) e->SetClass(name, on);
			remember(document, Applied{ Applied::Kind::Class, element, name, std::string(), on });
		}
	}

	void BgfxRmlUi::set_property(unsigned document, const std::string& element, const std::string& name, const std::string& value)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (Rml::Element* e = find(document, element))
		{
			e->SetProperty(name, value);
			remember(document, Applied{ Applied::Kind::Property, element, name, value });
		}
	}

	void BgfxRmlUi::scroll_by(unsigned document, const std::string& element, float dp)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (Rml::Element* e = find(document, element)) e->SetScrollTop(e->GetScrollTop() + dp * m_context->GetDensityIndependentPixelRatio());
	}

	void BgfxRmlUi::scroll_into_view(unsigned document, const std::string& element)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (Rml::Element* e = find(document, element))
			e->ScrollIntoView(Rml::ScrollIntoViewOptions(Rml::ScrollAlignment::Adaptive, Rml::ScrollAlignment::Nearest, Rml::ScrollBehavior::Instant, Rml::ScrollParentage::Closest));
	}

	// The buttons of a document a key or a pad can reach: the focusable elements (tab-index: auto) that are shown, in the order of
	// the document; inside the last shown "nav-trap" when there is one (a dialog over the screen)
	static void nav_buttons(Rml::Element* root, std::vector<Rml::Element*>& buttons)
	{
		Rml::ElementList traps;
		root->GetElementsByClassName(traps, "nav-trap");
		for (auto trap = traps.rbegin(); trap != traps.rend(); ++trap)
		{
			if ((*trap)->IsVisible(true))
			{
				root = *trap;
				break;
			}
		}
		std::vector<Rml::Element*> pending{ root };
		while (!pending.empty())
		{
			Rml::Element* parent = pending.back();
			pending.pop_back();
			// (children pushed last to first: the document's order comes out)
			for (int i = parent->GetNumChildren() - 1; i >= 0; --i)
			{
				Rml::Element* child = parent->GetChild(i);
				if (!child->IsVisible()) continue;   // hidden (display: none, a data-if): nor are its children shown
				pending.push_back(child);
			}
			if (parent != root && parent->GetComputedValues().tab_index() == Rml::Style::TabIndex::Auto
				&& parent->GetBox().GetSize(Rml::BoxArea::Border).x > 0.0f)
			{
				buttons.push_back(parent);
			}
		}
	}

	static Rml::Vector2f nav_center(Rml::Element* element)
	{
		return element->GetAbsoluteOffset(Rml::BoxArea::Border) + element->GetBox().GetSize(Rml::BoxArea::Border) * 0.5f;
	}

	void BgfxRmlUi::navigate(unsigned document, NavDirection direction)
	{
		InputEvent e{ InputEvent::Type::Navigate };
		e.document = document;
		e.direction = direction;
		push(e);
	}

	void BgfxRmlUi::activate(unsigned document)
	{
		InputEvent e{ InputEvent::Type::Activate };
		e.document = document;
		push(e);
	}

	void BgfxRmlUi::navigate_now(unsigned document, NavDirection direction)
	{
		const auto found = m_documents.find(document);
		if (found == m_documents.end() || !found->second->IsVisible()) return;
		std::vector<Rml::Element*> buttons;
		nav_buttons(found->second, buttons);
		if (buttons.empty()) return;
		Rml::Element* focused = m_context->GetFocusElement();
		Rml::Element* next = nullptr;
		if (std::find(buttons.begin(), buttons.end(), focused) == buttons.end())
		{
			next = buttons.front();   // none has the focus yet: the first one takes it
		}
		else
		{
			// The nearest button that way: its distance along the way, and across it counted twice (a button straight ahead
			// wins over a closer one off to the side)
			const Rml::Vector2f way = direction == NavDirection::Up ? Rml::Vector2f(0.0f, -1.0f)
				: direction == NavDirection::Down ? Rml::Vector2f(0.0f, 1.0f)
				: direction == NavDirection::Left ? Rml::Vector2f(-1.0f, 0.0f) : Rml::Vector2f(1.0f, 0.0f);
			const Rml::Vector2f from = nav_center(focused);
			float best = 0.0f;
			for (Rml::Element* button : buttons)
			{
				if (button == focused) continue;
				const Rml::Vector2f offset = nav_center(button) - from;
				const float along = offset.x * way.x + offset.y * way.y;
				if (along < 1.0f) continue;
				const float score = along + 2.0f * std::abs(offset.x * way.y - offset.y * way.x);
				if (!next || score < best)
				{
					next = button;
					best = score;
				}
			}
		}
		if (!next) return;
		next->Focus(true);
		next->ScrollIntoView(Rml::ScrollIntoViewOptions(Rml::ScrollAlignment::Nearest, Rml::ScrollAlignment::Nearest, Rml::ScrollBehavior::Instant,
			Rml::ScrollParentage::All));
	}

	void BgfxRmlUi::focus_first(unsigned document)
	{
		InputEvent e{ InputEvent::Type::FocusFirst };
		e.document = document;
		push(e);
	}

	void BgfxRmlUi::focus_first_now(unsigned document)
	{
		const auto found = m_documents.find(document);
		if (found == m_documents.end() || !found->second->IsVisible()) return;
		std::vector<Rml::Element*> buttons;
		nav_buttons(found->second, buttons);
		if (buttons.empty() || std::find(buttons.begin(), buttons.end(), m_context->GetFocusElement()) != buttons.end()) return;
		buttons.front()->Focus(true);
		buttons.front()->ScrollIntoView(Rml::ScrollIntoViewOptions(Rml::ScrollAlignment::Nearest, Rml::ScrollAlignment::Nearest,
			Rml::ScrollBehavior::Instant, Rml::ScrollParentage::All));
	}

	void BgfxRmlUi::activate_now(unsigned document)
	{
		const auto found = m_documents.find(document);
		if (found == m_documents.end() || !found->second->IsVisible()) return;
		std::vector<Rml::Element*> buttons;
		nav_buttons(found->second, buttons);
		Rml::Element* focused = m_context->GetFocusElement();
		if (std::find(buttons.begin(), buttons.end(), focused) != buttons.end()) focused->Click();
	}

	void BgfxRmlUi::set_attribute(unsigned document, const std::string& element, const std::string& name, const std::string& value)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (Rml::Element* e = find(document, element))
		{
			e->SetAttribute(name, value);
			remember(document, Applied{ Applied::Kind::Attribute, element, name, value });
		}
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

	float BgfxRmlUi::dp_ratio() const
	{
		return m_width && m_height ? design_ratio(m_width, m_height) : 1.0f;
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
			case InputEvent::Type::Navigate: navigate_now(e.document, e.direction); break;
			case InputEvent::Type::Activate: activate_now(e.document); break;
			case InputEvent::Type::FocusFirst: focus_first_now(e.document); break;
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
			m_context->SetDensityIndependentPixelRatio(design_ratio(width, height));
		}
		feed_events();
		update_build_label();
		m_context->Update();
		m_backend->begin_frame(width, height);
		m_context->Render();
	}
	void BgfxRmlUi::set_build_label(const std::string& text, BuildLabelCorner corner)
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (text == m_label_text && corner == m_label_corner) return;
		m_label_text = text;
		m_label_corner = corner;
		m_label_dirty = true;
	}

	std::string BgfxRmlUi::undrawn_build_label(BuildLabelCorner* corner) const
	{
		std::lock_guard<std::recursive_mutex> guard(m_lock);
		if (m_label_document || m_label_text.empty()) return {};
		if (corner) *corner = m_label_corner;
		return m_label_text;
	}

	void BgfxRmlUi::update_build_label()
	{
		// a document of its own, made again when the text, the corner or the font changes; kept over every other document
		if (m_label_dirty)
		{
			m_label_dirty = false;
			if (m_label_document)
			{
				m_label_document->Close();
				m_label_document = nullptr;
			}
			if (!m_label_text.empty() && !m_label_font.empty())
			{
				const bool top = m_label_corner == BuildLabelCorner::TopLeft || m_label_corner == BuildLabelCorner::TopRight;
				const bool left = m_label_corner == BuildLabelCorner::TopLeft || m_label_corner == BuildLabelCorner::BottomLeft;
				std::string escaped;
				for (const char c : m_label_text)
				{
					if (c == '<') escaped += "&lt;";
					else if (c == '>') escaped += "&gt;";
					else if (c == '&') escaped += "&amp;";
					else escaped += c;
				}
				const std::string rml = std::string("<rml><head><style>body { position: absolute; left: 0; top: 0; right: 0; bottom: 0; "
					"pointer-events: none; font-family: \"") + m_label_font + "\"; } "
					"div { position: absolute; " + (top ? "top" : "bottom") + ": 4dp; " + (left ? "left" : "right") + ": 6dp; "
					"font-size: 11dp; white-space: pre; color: #ffffffcc; font-effect: outline(1dp #000000aa); pointer-events: none; }"
					"</style></head><body><div>" + escaped + "</div></body></rml>";
				m_label_document = m_context->LoadDocumentFromMemory(rml, "[build label]");
				if (m_label_document) m_label_document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
				else LOG_WARNING("[UI] the build label did not load");
			}
		}
		if (m_label_document) m_label_document->PullToFront();
	}
}
