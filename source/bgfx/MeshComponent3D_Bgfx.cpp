///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: MeshComponent3D_Bgfx.cpp
//
// Purpose: 3D mesh rendering component using bgfx - Implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "MeshComponent3D_Bgfx.h"
#include "BgfxGraphicsSystem.h"
#include "LogMgr.h"
#include <cstdlib>
#include <persist/pugixml/pugixml.hpp>

#include <assimp/Importer.hpp>
#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include "ResourceManager.h"
#include <chrono>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/GltfMaterial.h>

#include <bgfx/bgfx.h>
#include <bimg/decode.h>
#include <bx/allocator.h>
#include <bx/math.h>
#include <meshoptimizer.h>

#include <algorithm>
#include <map>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <mutex>

namespace fs = std::filesystem;

namespace Pakal
{
	namespace
	{
		bgfx::UniformHandle s_texColorSampler = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle s_tintUniform = BGFX_INVALID_HANDLE;     // u_tint: color multiplier
		bgfx::UniformHandle s_uvScrollUniform = BGFX_INVALID_HANDLE; // u_uvScroll: xy, the texture's offset now; z, w: water, its clock
		bgfx::UniformHandle s_groundUniform = BGFX_INVALID_HANDLE;   // u_ground: x, y: the ground's detiling and macro variation
		bgfx::UniformHandle s_shadowMtxUniform = BGFX_INVALID_HANDLE;    // u_shadowMtx
		bgfx::UniformHandle s_shadowParamsUniform = BGFX_INVALID_HANDLE; // u_shadowParams
		bgfx::UniformHandle s_shadowSampler = BGFX_INVALID_HANDLE;       // s_shadowMap
		bgfx::UniformHandle s_normalSampler = BGFX_INVALID_HANDLE;       // s_texNormal
		bgfx::UniformHandle s_ormSampler = BGFX_INVALID_HANDLE;          // s_texOrm
		bgfx::UniformHandle s_giSampler = BGFX_INVALID_HANDLE;           // s_giVolume
		bgfx::UniformHandle s_giOriginUniform = BGFX_INVALID_HANDLE;     // u_giOrigin
		bgfx::UniformHandle s_giSizeUniform = BGFX_INVALID_HANDLE;       // u_giSize
		bgfx::TextureHandle s_giVolume = BGFX_INVALID_HANDLE;
		float s_giOrigin[4] = {};
		float s_giSize[4] = {};
		bgfx::UniformHandle s_lightmapSampler = BGFX_INVALID_HANDLE;     // s_lightmap
		bgfx::UniformHandle s_lightmapUniform = BGFX_INVALID_HANDLE;     // u_lightmap
		bgfx::UniformHandle s_styleUniform = BGFX_INVALID_HANDLE;        // u_style
		bgfx::UniformHandle s_paintSampler = BGFX_INVALID_HANDLE;        // s_texPaint: where the walls and roof are (set_paint)
		bgfx::UniformHandle s_paintWallUniform = BGFX_INVALID_HANDLE;    // u_paintWall: rgb, w 1 painted
		bgfx::UniformHandle s_paintRoofUniform = BGFX_INVALID_HANDLE;    // u_paintRoof
		float s_style[4] = { 0.0f, 32.0f, 0.0f, 0.0f };
		bgfx::TextureHandle s_lightmapAtlas = BGFX_INVALID_HANDLE;
		float s_lightmapShadow = 0.0f;
		bgfx::TextureHandle s_flatNormalTexture = BGFX_INVALID_HANDLE;   // a normal map that changes nothing
		bgfx::TextureHandle s_plainOrmTexture = BGFX_INVALID_HANDLE;     // occlusion 1, roughness 1, metal 0
		// A 1x1x1 volume for the light volume's sampler when the level has none: a draw whose 3D sampler has nothing bound
		// draws nothing on some OpenGL ES drivers
		bgfx::TextureHandle s_dummyVolume = BGFX_INVALID_HANDLE;
		// What the shadow sampler holds when there is no map: a depth texture it can compare against (the shaders sample it
		// only with shadows on, but a comparison sampler left with nothing, or a color texture, made a phone's driver slow
		// every draw down)
		bgfx::TextureHandle s_dummyShadow = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle s_depthProgram = BGFX_INVALID_HANDLE; // owned by the graphics system
		bgfx::TextureHandle s_shadowMap = BGFX_INVALID_HANDLE;
		float s_shadowMatrix[16] = {};
		float s_shadowParams[4] = {};

		struct CullFrustum
		{
			bool valid = false;
			float planes[6][4] = {};
		};
		CullFrustum s_cullFrusta[2];
		// The camera's axis (the view matrix's third row): a point's distance in front of it
		float s_sortRow[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		bool s_sortValid = false;
		float s_shadowReach = -1.0f;   // negative: every model in the sun's view casts
		float s_cullEye[3] = { 0.0f, 0.0f, 0.0f };   // the camera's position (from its view matrix)
		float s_drawPerMeter = 0.0f, s_drawMinimum = 0.0f;   // set_draw_distance: 0, off
		std::atomic<bool> s_gridMoved{ false };   // a model in the graphics system's grid moved (take_grid_moved)
		// The shadow casters: positions only
		bgfx::VertexLayout s_positionLayout;
		// What each pass drew since the last take_pass_stats: shadow, opaque, transparent
		MeshComponent3D_Bgfx::PassStats s_passStats[3];
		bgfx::UniformHandle s_materialUniform = BGFX_INVALID_HANDLE; // u_material: alpha ref, keep alpha, unlit
		uint32_t s_meshInstanceCount = 0;
		bgfx::TextureHandle s_whiteTexture = BGFX_INVALID_HANDLE;

		const float kOverlayBlendSeconds = 0.1f; // an overlay clip blends in and out over this long

		// Bone palette size; must match BGFX_CONFIG_MAX_BONES in shaders/vs_skin.sc
		const size_t kMaxPaletteEntries = 64;
		const std::string s_emptyName;

		void acquire_shared_handles()
		{
			if (s_meshInstanceCount++ == 0)
			{
				s_texColorSampler = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
				s_tintUniform = bgfx::createUniform("u_tint", bgfx::UniformType::Vec4);
				s_uvScrollUniform = bgfx::createUniform("u_uvScroll", bgfx::UniformType::Vec4);
				s_groundUniform = bgfx::createUniform("u_ground", bgfx::UniformType::Vec4);
				s_shadowMtxUniform = bgfx::createUniform("u_shadowMtx", bgfx::UniformType::Mat4);
				s_shadowParamsUniform = bgfx::createUniform("u_shadowParams", bgfx::UniformType::Vec4);
				s_shadowSampler = bgfx::createUniform("s_shadowMap", bgfx::UniformType::Sampler);
				s_normalSampler = bgfx::createUniform("s_texNormal", bgfx::UniformType::Sampler);
				s_giSampler = bgfx::createUniform("s_giVolume", bgfx::UniformType::Sampler);
				s_giOriginUniform = bgfx::createUniform("u_giOrigin", bgfx::UniformType::Vec4);
				s_giSizeUniform = bgfx::createUniform("u_giSize", bgfx::UniformType::Vec4);
				s_ormSampler = bgfx::createUniform("s_texOrm", bgfx::UniformType::Sampler);
				s_lightmapSampler = bgfx::createUniform("s_lightmap", bgfx::UniformType::Sampler);
				s_lightmapUniform = bgfx::createUniform("u_lightmap", bgfx::UniformType::Vec4);
				s_styleUniform = bgfx::createUniform("u_style", bgfx::UniformType::Vec4);
				s_paintSampler = bgfx::createUniform("s_texPaint", bgfx::UniformType::Sampler);
				s_paintWallUniform = bgfx::createUniform("u_paintWall", bgfx::UniformType::Vec4);
				s_paintRoofUniform = bgfx::createUniform("u_paintRoof", bgfx::UniformType::Vec4);
				{
					const uint8_t flat[4] = { 128, 128, 255, 255 };
					const uint8_t plain[4] = { 255, 255, 0, 255 };
					s_flatNormalTexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, bgfx::copy(flat, 4));
					s_plainOrmTexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, bgfx::copy(plain, 4));
					const uint8_t white4[4] = { 255, 255, 255, 255 };
					if (bgfx::getCaps()->supported & BGFX_CAPS_TEXTURE_3D)
					{
						s_dummyVolume = bgfx::createTexture3D(1, 1, 1, false, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, bgfx::copy(white4, 4));
					}
				}
				s_dummyShadow = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::D16, BGFX_TEXTURE_RT | BGFX_SAMPLER_COMPARE_LEQUAL);
				s_materialUniform = bgfx::createUniform("u_material", bgfx::UniformType::Vec4);
				const uint32_t white = 0xffffffff;
				s_whiteTexture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8,
					BGFX_TEXTURE_NONE, bgfx::copy(&white, sizeof(white)));
			}
		}

		void release_shared_handles()
		{
			if (s_meshInstanceCount == 0)
			{
				return;
			}
			if (--s_meshInstanceCount == 0)
			{
				if (bgfx::isValid(s_texColorSampler)) bgfx::destroy(s_texColorSampler);
				if (bgfx::isValid(s_tintUniform)) bgfx::destroy(s_tintUniform);
				if (bgfx::isValid(s_shadowMtxUniform)) bgfx::destroy(s_shadowMtxUniform);
				if (bgfx::isValid(s_shadowParamsUniform)) bgfx::destroy(s_shadowParamsUniform);
				if (bgfx::isValid(s_shadowSampler)) bgfx::destroy(s_shadowSampler);
				if (bgfx::isValid(s_giSampler)) bgfx::destroy(s_giSampler);
				if (bgfx::isValid(s_giOriginUniform)) bgfx::destroy(s_giOriginUniform);
				if (bgfx::isValid(s_giSizeUniform)) bgfx::destroy(s_giSizeUniform);
				s_giSampler = s_giOriginUniform = s_giSizeUniform = BGFX_INVALID_HANDLE;
				if (bgfx::isValid(s_lightmapSampler)) bgfx::destroy(s_lightmapSampler);
				if (bgfx::isValid(s_lightmapUniform)) bgfx::destroy(s_lightmapUniform);
				if (bgfx::isValid(s_styleUniform)) bgfx::destroy(s_styleUniform);
				s_styleUniform = BGFX_INVALID_HANDLE;
				if (bgfx::isValid(s_paintSampler)) bgfx::destroy(s_paintSampler);
				if (bgfx::isValid(s_paintWallUniform)) bgfx::destroy(s_paintWallUniform);
				if (bgfx::isValid(s_paintRoofUniform)) bgfx::destroy(s_paintRoofUniform);
				s_paintSampler = s_paintWallUniform = s_paintRoofUniform = BGFX_INVALID_HANDLE;
				s_lightmapSampler = s_lightmapUniform = BGFX_INVALID_HANDLE;
				if (bgfx::isValid(s_normalSampler)) bgfx::destroy(s_normalSampler);
				if (bgfx::isValid(s_ormSampler)) bgfx::destroy(s_ormSampler);
				if (bgfx::isValid(s_flatNormalTexture)) bgfx::destroy(s_flatNormalTexture);
				if (bgfx::isValid(s_plainOrmTexture)) bgfx::destroy(s_plainOrmTexture);
				if (bgfx::isValid(s_dummyVolume)) bgfx::destroy(s_dummyVolume);
				s_dummyVolume = BGFX_INVALID_HANDLE;
				if (bgfx::isValid(s_dummyShadow)) bgfx::destroy(s_dummyShadow);
				s_dummyShadow = BGFX_INVALID_HANDLE;
				s_shadowMtxUniform = s_shadowParamsUniform = s_shadowSampler = BGFX_INVALID_HANDLE;
				s_normalSampler = s_ormSampler = BGFX_INVALID_HANDLE;
				s_flatNormalTexture = s_plainOrmTexture = BGFX_INVALID_HANDLE;
				if (bgfx::isValid(s_materialUniform)) bgfx::destroy(s_materialUniform);
				if (bgfx::isValid(s_uvScrollUniform)) bgfx::destroy(s_uvScrollUniform);
				s_uvScrollUniform = BGFX_INVALID_HANDLE;
				if (bgfx::isValid(s_groundUniform)) bgfx::destroy(s_groundUniform);
				s_groundUniform = BGFX_INVALID_HANDLE;
				s_tintUniform = BGFX_INVALID_HANDLE;
				s_materialUniform = BGFX_INVALID_HANDLE;
				if (bgfx::isValid(s_whiteTexture)) bgfx::destroy(s_whiteTexture);
				s_texColorSampler = BGFX_INVALID_HANDLE;
				s_whiteTexture = BGFX_INVALID_HANDLE;
			}
		}

		// Lower-case file extension of a path, with the dot (".gltf")
		std::string lower_extension(const std::string& path)
		{
			std::string extension = std::filesystem::path(path).extension().string();
			for (char& c : extension)
			{
				c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			}
			return extension;
		}

		bgfx::TextureHandle load_texture_memory(const uint8_t* bytes, size_t size);
		bimg::ImageContainer* parse_image(const uint8_t* bytes, size_t size);
		bimg::ImageContainer* parse_image_file(const std::string& path);
		bgfx::TextureHandle create_texture(bimg::ImageContainer* image);
		bgfx::TextureHandle create_texture_from(const bimg::ImageContainer* image);

		// Textures loaded from files, shared by every model that names the same file (models merged by area share their
		// textures): the handle and how many submeshes use it
		struct CachedTexture
		{
			bgfx::TextureHandle handle;
			int users;
		};
		std::mutex s_textureMutex;
		std::map<std::string, CachedTexture> s_textureCache;

		// Where loading time goes (MeshComponent3D_Bgfx::take_load_stats)
		struct LoadStats { double total = 0, import = 0, textures = 0, shadow = 0; int models = 0, textures_decoded = 0; };
		LoadStats s_stats;
		std::mutex s_statsMutex;   // models decode on several threads at once (AssetCache)
		struct StatTimer
		{
			double& into;
			std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
			explicit StatTimer(double& target) : into(target) {}
			~StatTimer()
			{
				const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
				std::lock_guard<std::mutex> lock(s_statsMutex);
				into += ms;
			}
		};
		void count_stat(int& counter)
		{
			std::lock_guard<std::mutex> lock(s_statsMutex);
			++counter;
		}

		bgfx::TextureHandle decode_texture_file(const std::string& path);

		// Decodes an image file (PNG, DDS, KTX, ...) into a texture, or gives the one already loaded from that file.
		// Returns an invalid handle on failure. release_texture() gives it back.
		bgfx::TextureHandle load_texture_file(const std::string& path)
		{
			std::lock_guard<std::mutex> lock(s_textureMutex);
			auto found = s_textureCache.find(path);
			if (found != s_textureCache.end())
			{
				++found->second.users;
				return found->second.handle;
			}
			const bgfx::TextureHandle handle = decode_texture_file(path);
			if (bgfx::isValid(handle))
			{
				s_textureCache[path] = { handle, 1 };
			}
			return handle;
		}

		// An image file decoded for models being loaded in the background: the first model that names it decodes it, the ones
		// that name it meanwhile wait for that and share it; the first upload makes the texture and caches it
		struct SharedImage
		{
			bimg::ImageContainer* image = nullptr;
			~SharedImage() { if (image) bimg::imageFree(image); }
		};
		std::mutex s_decodingMutex;
		std::map<std::string, std::shared_future<std::shared_ptr<SharedImage>>> s_decoding;

		std::shared_ptr<SharedImage> decode_shared(const std::string& path)
		{
			std::promise<std::shared_ptr<SharedImage>> promise;
			std::shared_future<std::shared_ptr<SharedImage>> future;
			bool mine = false;
			{
				std::lock_guard<std::mutex> lock(s_decodingMutex);
				auto found = s_decoding.find(path);
				if (found != s_decoding.end())
				{
					future = found->second;
				}
				else
				{
					future = promise.get_future().share();
					s_decoding[path] = future;
					mine = true;
				}
			}
			if (mine)
			{
				auto shared = std::make_shared<SharedImage>();
				shared->image = parse_image_file(path);
				promise.set_value(shared);
			}
			return future.get();
		}

		bool texture_cached(const std::string& path)
		{
			std::lock_guard<std::mutex> lock(s_textureMutex);
			return s_textureCache.count(path) != 0;
		}

		// The texture of a file for a model being uploaded: the one already loaded, or one from its decoded image (or read
		// now when there is none: the one cached while it decoded is gone)
		bgfx::TextureHandle cache_texture(const std::string& path, const SharedImage* image)
		{
			std::lock_guard<std::mutex> lock(s_textureMutex);
			auto found = s_textureCache.find(path);
			if (found != s_textureCache.end())
			{
				++found->second.users;
				return found->second.handle;
			}
			const bgfx::TextureHandle handle = image && image->image ? create_texture_from(image->image) : decode_texture_file(path);
			if (bgfx::isValid(handle))
			{
				s_textureCache[path] = { handle, 1 };
			}
			// cached now: the next models take it from the cache, not from a decoded image
			std::lock_guard<std::mutex> decoding(s_decodingMutex);
			s_decoding.erase(path);
			return handle;
		}

		// A texture is destroyed when its last user lets it go (one decoded from memory has only one user)
		void release_texture(bgfx::TextureHandle handle)
		{
			std::lock_guard<std::mutex> lock(s_textureMutex);
			for (auto it = s_textureCache.begin(); it != s_textureCache.end(); ++it)
			{
				if (it->second.handle.idx == handle.idx)
				{
					if (--it->second.users <= 0)
					{
						bgfx::destroy(handle);
						s_textureCache.erase(it);
					}
					return;
				}
			}
			bgfx::destroy(handle);
		}

		bgfx::TextureHandle decode_texture_file(const std::string& path)
		{
			// from wherever the resource manager finds it (a folder, a zip, the APK)
			const auto data = ResourceMgr.read_bytes(path);
			if (!data || data->empty())
			{
				return BGFX_INVALID_HANDLE;
			}
			return load_texture_memory(data->data(), data->size());
		}

		// Decodes an image held in memory (a file's bytes, or a texture embedded in a model)
		bgfx::TextureHandle load_texture_memory(const uint8_t* bytes, size_t size)
		{
			bimg::ImageContainer* image = parse_image(bytes, size);
			if (!image) return BGFX_INVALID_HANDLE;
			return create_texture(image);
		}

		// Decodes an image (any thread). imageFree() gives it back; create_texture() does too.
		bimg::ImageContainer* parse_image(const uint8_t* bytes, size_t size)
		{
			static bx::DefaultAllocator allocator;   // the image keeps it until it is freed
			StatTimer timer(s_stats.textures);
			count_stat(s_stats.textures_decoded);
			return bimg::imageParse(&allocator, bytes, static_cast<uint32_t>(size));
		}

		bimg::ImageContainer* parse_image_file(const std::string& path)
		{
			const auto data = ResourceMgr.read_bytes(path);
			if (!data || data->empty()) return nullptr;
			return parse_image(data->data(), data->size());
		}

		// Assimp reads a model and its buffers (a glTF's .bin) through the resource manager: from a folder, a zip or the APK
		class ResourceIOStream final : public Assimp::IOStream
		{
		public:
			explicit ResourceIOStream(SharedPtr<const std::vector<uint8_t>> bytes) : m_bytes(std::move(bytes)) {}
			size_t Read(void* buffer, size_t size, size_t count) override
			{
				if (size == 0) return 0;
				const size_t whole = std::min(count, (m_bytes->size() - m_at) / size);
				std::memcpy(buffer, m_bytes->data() + m_at, whole * size);
				m_at += whole * size;
				return whole;
			}
			size_t Write(const void*, size_t, size_t) override { return 0; }
			aiReturn Seek(size_t offset, aiOrigin origin) override
			{
				const size_t base = origin == aiOrigin_SET ? 0 : origin == aiOrigin_CUR ? m_at : m_bytes->size();
				if (base + offset > m_bytes->size()) return aiReturn_FAILURE;
				m_at = base + offset;
				return aiReturn_SUCCESS;
			}
			size_t Tell() const override { return m_at; }
			size_t FileSize() const override { return m_bytes->size(); }
			void Flush() override {}
		private:
			SharedPtr<const std::vector<uint8_t>> m_bytes;
			size_t m_at = 0;
		};

		class ResourceIOSystem final : public Assimp::IOSystem
		{
		public:
			bool Exists(const char* file) const override { return ResourceMgr.exists(normal(file)); }
			char getOsSeparator() const override { return '/'; }
			Assimp::IOStream* Open(const char* file, const char* mode) override
			{
				if (mode && (std::strchr(mode, 'w') || std::strchr(mode, 'a'))) return nullptr;   // models are only read
				auto bytes = ResourceMgr.read_bytes(normal(file));
				return bytes ? new ResourceIOStream(std::move(bytes)) : nullptr;
			}
			void Close(Assimp::IOStream* stream) override { delete stream; }
		private:
			static std::string normal(const char* file) { return fs::path(file).lexically_normal().generic_string(); }
		};

		// A texture from a decoded image (the graphics thread); the image is freed
		bgfx::TextureHandle create_texture(bimg::ImageContainer* image)
		{
			const bgfx::TextureHandle texture = create_texture_from(image);
			bimg::imageFree(image);
			return texture;
		}

		// ... and one that leaves the image as it is
		bgfx::TextureHandle create_texture_from(const bimg::ImageContainer* image)
		{
			const bgfx::Memory* mem = bgfx::copy(image->m_data, image->m_size);
			// A color map of a few texels (a palette: each face samples one cell of 32x32) is read without filtering: blended,
			// the cells bleed into each other
			const bool palette = image->m_width <= 64 && image->m_height <= 64;
			bgfx::TextureHandle texture = bgfx::createTexture2D(
				static_cast<uint16_t>(image->m_width),
				static_cast<uint16_t>(image->m_height),
				image->m_numMips > 1,
				image->m_numLayers,
				static_cast<bgfx::TextureFormat::Enum>(image->m_format),
				palette ? uint64_t(BGFX_SAMPLER_POINT) : uint64_t(BGFX_TEXTURE_NONE),
				mem);
			return texture;
		}

		// Resolves a texture path from the model. Exporters write absolute or foreign paths, so
		// the file name is also tried next to the model. A packed copy, the same name with .ktx
		// added (compressed, with mips: android/pack_textures.py), wins over the original, which
		// the package may leave out.
		std::string resolve_texture_path(const fs::path& modelDir, const std::string& texturePath)
		{
			// lexically normal: models that share a texture folder (meshes/<model>/../_shared/...) name the same file, and the
			// texture cache is keyed by this path
			for (const fs::path candidate : { (modelDir / fs::path(texturePath).relative_path()).lexically_normal(),
					(modelDir / fs::path(texturePath).filename()).lexically_normal() })
			{
				fs::path packed = candidate;
				packed += ".ktx";
				if (ResourceMgr.exists(packed.generic_string()))
				{
					return packed.generic_string();
				}
				if (ResourceMgr.exists(candidate.generic_string()))
				{
					return candidate.generic_string();
				}
			}
			return std::string();
		}

		// Column-major float matrix (bgfx / bx layout) from an Assimp row-major matrix.
		void write_column_major(const aiMatrix4x4& m, float* out)
		{
			for (int c = 0; c < 4; ++c)
			{
				for (int r = 0; r < 4; ++r)
				{
					out[c * 4 + r] = m[r][c];
				}
			}
		}

		aiMatrix4x4 read_column_major(const float* in)
		{
			aiMatrix4x4 m;
			for (int c = 0; c < 4; ++c)
			{
				for (int r = 0; r < 4; ++r)
				{
					m[r][c] = in[c * 4 + r];
				}
			}
			return m;
		}

		// Linear interpolation between keys. Returns the first or last key outside the key range.
		aiVector3D sample_vector(const aiVectorKey* keys, unsigned int count, double time, const aiVector3D& fallback)
		{
			if (count == 0)
			{
				return fallback;
			}
			if (count == 1 || time <= keys[0].mTime)
			{
				return keys[0].mValue;
			}
			if (time >= keys[count - 1].mTime)
			{
				return keys[count - 1].mValue;
			}

			unsigned int i = 0;
			while (i + 1 < count && time >= keys[i + 1].mTime)
			{
				++i;
			}
			const double span = keys[i + 1].mTime - keys[i].mTime;
			const float f = span > 0.0 ? static_cast<float>((time - keys[i].mTime) / span) : 0.0f;
			const aiVector3D& a = keys[i].mValue;
			const aiVector3D& b = keys[i + 1].mValue;
			return aiVector3D(a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.z + (b.z - a.z) * f);
		}

		aiQuaternion sample_rotation(const aiQuatKey* keys, unsigned int count, double time, const aiQuaternion& fallback)
		{
			if (count == 0)
			{
				return fallback;
			}
			if (count == 1 || time <= keys[0].mTime)
			{
				return keys[0].mValue;
			}
			if (time >= keys[count - 1].mTime)
			{
				return keys[count - 1].mValue;
			}

			unsigned int i = 0;
			while (i + 1 < count && time >= keys[i + 1].mTime)
			{
				++i;
			}
			const double span = keys[i + 1].mTime - keys[i].mTime;
			const float f = span > 0.0 ? static_cast<float>((time - keys[i].mTime) / span) : 0.0f;
			aiQuaternion out;
			aiQuaternion::Interpolate(out, keys[i].mValue, keys[i + 1].mValue, f);
			out.Normalize();
			return out;
		}
	}

	struct MeshComponent3D_Bgfx::PendingUpload
	{
		std::vector<Vertex> staticVertices;
		std::vector<SkinnedVertex> skinnedVertices;
		std::vector<uint32_t> indices;
		std::vector<float> shadowVertices;
		std::vector<uint32_t> shadowIndices;
		// A submesh's texture ('slot' 0 color, 1 normal, 2 ORM): its file (cached by it) and the decoded image shared with
		// the other models of that file (none: take the cached one), or an image embedded in the model, its own
		struct Texture
		{
			size_t sub = 0;
			int slot = 0;
			std::string path;
			std::shared_ptr<SharedImage> shared;
			bimg::ImageContainer* image = nullptr;
		};
		std::vector<Texture> textures;

		~PendingUpload()
		{
			for (auto& texture : textures)
			{
				if (texture.image) bimg::imageFree(texture.image);
			}
		}
	};

	bgfx::VertexLayout MeshComponent3D_Bgfx::Vertex::ms_layout;
	bgfx::VertexLayout MeshComponent3D_Bgfx::SkinnedVertex::ms_layout;

	void MeshComponent3D_Bgfx::Vertex::init()
	{
		ms_layout
			.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Tangent, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord1, 2, bgfx::AttribType::Float)
			.end();
	}

	// The solid materials of a static model as one mesh for the sun's shadow map: positions only, welded (texture seams
	// do not split it) and simplified, since the map is coarser than the screen. One draw instead of one per material.
	void MeshComponent3D_Bgfx::build_shadow_mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::string& path)
	{
		StatTimer timer(s_stats.shadow);
		std::vector<uint32_t> solid;
		for (const auto& sub : m_subMeshes)
		{
			if (sub.blend == MaterialBlend::Opaque || sub.blend == MaterialBlend::AlphaTest)
			{
				solid.insert(solid.end(), indices.begin() + sub.indexStart, indices.begin() + sub.indexStart + sub.indexCount);
			}
		}
		if (solid.empty())
		{
			return;
		}
		std::vector<float> positions(vertices.size() * 3);
		for (size_t i = 0; i < vertices.size(); ++i)
		{
			positions[i * 3 + 0] = vertices[i].x;
			positions[i * 3 + 1] = vertices[i].y;
			positions[i * 3 + 2] = vertices[i].z;
		}
		std::vector<uint32_t> remap(vertices.size());
		const size_t unique = meshopt_generateVertexRemap(remap.data(), solid.data(), solid.size(), positions.data(), vertices.size(), sizeof(float) * 3);
		std::vector<float> welded(unique * 3);
		meshopt_remapVertexBuffer(welded.data(), positions.data(), vertices.size(), sizeof(float) * 3, remap.data());
		meshopt_remapIndexBuffer(solid.data(), solid.data(), solid.size(), remap.data());

		// Models with few triangles keep them all; bigger ones go down to a quarter, with at most 1% of their size of error
		const size_t sourceTriangles = solid.size() / 3;
		if (sourceTriangles > 2000)
		{
			std::vector<uint32_t> simplified(solid.size());
			const size_t count = meshopt_simplify(simplified.data(), solid.data(), solid.size(), welded.data(), unique, sizeof(float) * 3,
				solid.size() / 4, 0.01f, 0, nullptr);
			if (count >= 3)
			{
				simplified.resize(count);
				solid.swap(simplified);
			}
			// Loose cards (a tree's leaves) share no edges to collapse and stay nearly whole: those are clustered instead,
			// a blob of the crown is enough for the map
			const size_t target = sourceTriangles * 3 / 4;
			if (solid.size() > target * 3 / 2)
			{
				std::vector<uint32_t> sloppy(solid.size());
				const size_t sloppyCount = meshopt_simplifySloppy(sloppy.data(), solid.data(), solid.size(), welded.data(), unique,
					sizeof(float) * 3, nullptr, target, 0.08f, nullptr);
				if (sloppyCount >= 3)
				{
					sloppy.resize(sloppyCount);
					solid.swap(sloppy);
				}
			}
		}
		std::vector<float> compact(unique * 3);
		const size_t used = meshopt_optimizeVertexFetch(compact.data(), solid.data(), solid.size(), welded.data(), unique, sizeof(float) * 3);
		compact.resize(used * 3);

		m_shadowIndexCount = static_cast<uint32_t>(solid.size());
		if (sourceTriangles > 2000)
		{
			LOG_INFO("[MeshComponent3D_Bgfx] '%s' casts shadows with %zu of its %zu triangles", path.c_str(), solid.size() / 3, sourceTriangles);
		}
		if (m_pending)
		{
			m_pending->shadowVertices = std::move(compact);
			m_pending->shadowIndices = std::move(solid);
			return;
		}
		if (s_positionLayout.getStride() == 0)
		{
			s_positionLayout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
		}
		m_shadowVertexBuffer = bgfx::createVertexBuffer(bgfx::copy(compact.data(), static_cast<uint32_t>(compact.size() * sizeof(float))), s_positionLayout);
		m_shadowIndexBuffer = bgfx::createIndexBuffer(bgfx::copy(solid.data(), static_cast<uint32_t>(solid.size() * sizeof(uint32_t))), BGFX_BUFFER_INDEX32);
	}

	// A skinned model's shadow: its solid triangles simplified over its own vertices (welded by position first, so texture
	// seams do not stop the simplification), skinned by the same palette. The characters cast a map coarser than they are
	// drawn: a town's crowd went into the sun's map with every triangle (a phone skinned them all twice a frame)
	void MeshComponent3D_Bgfx::build_skinned_shadow(const std::vector<SkinnedVertex>& vertices, const std::vector<uint32_t>& indices, const std::string& path)
	{
		StatTimer timer(s_stats.shadow);
		std::vector<uint32_t> solid;
		for (const auto& sub : m_subMeshes)
		{
			if (sub.blend == MaterialBlend::Opaque || sub.blend == MaterialBlend::AlphaTest)
			{
				solid.insert(solid.end(), indices.begin() + sub.indexStart, indices.begin() + sub.indexStart + sub.indexCount);
			}
		}
		const size_t sourceTriangles = solid.size() / 3;
		if (sourceTriangles <= 2000 || vertices.empty())
		{
			return;   // drawn whole, by its materials
		}
		std::vector<uint32_t> welded(solid.size());
		meshopt_generateShadowIndexBuffer(welded.data(), solid.data(), solid.size(), &vertices[0].x, vertices.size(), sizeof(float) * 3,
			sizeof(SkinnedVertex));
		std::vector<uint32_t> simplified(welded.size());
		const size_t count = meshopt_simplify(simplified.data(), welded.data(), welded.size(), &vertices[0].x, vertices.size(),
			sizeof(SkinnedVertex), welded.size() / 4, 0.01f, 0, nullptr);
		if (count < 3)
		{
			return;
		}
		simplified.resize(count);
		// A character is many loose pieces (clothes over the body, their borders): too few shared edges to collapse. Those
		// are clustered instead, down to a quarter: the map sees a silhouette some texels wide
		const size_t target = sourceTriangles / 4;
		if (simplified.size() / 3 > target * 3 / 2)
		{
			std::vector<uint32_t> sloppy(simplified.size());
			const size_t sloppyCount = meshopt_simplifySloppy(sloppy.data(), simplified.data(), simplified.size(), &vertices[0].x,
				vertices.size(), sizeof(SkinnedVertex), nullptr, target * 3, 0.02f, nullptr);
			if (sloppyCount >= 3)
			{
				sloppy.resize(sloppyCount);
				simplified.swap(sloppy);
			}
		}
		m_shadowIndexCount = static_cast<uint32_t>(simplified.size());
		LOG_INFO("[MeshComponent3D_Bgfx] '%s' casts shadows with %zu of its %zu triangles", path.c_str(), simplified.size() / 3, sourceTriangles);
		if (m_pending)
		{
			m_pending->shadowIndices = std::move(simplified);
			return;
		}
		m_shadowIndexBuffer = bgfx::createIndexBuffer(bgfx::copy(simplified.data(), static_cast<uint32_t>(simplified.size() * sizeof(uint32_t))), BGFX_BUFFER_INDEX32);
	}

	void MeshComponent3D_Bgfx::SkinnedVertex::init()
	{
		ms_layout
			.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Indices, 4, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Weight, 4, bgfx::AttribType::Float)
			.end();
	}

	MeshComponent3D_Bgfx::MeshComponent3D_Bgfx(BgfxGraphicsSystem* graphicsSystem) : MeshComponent3D_Bgfx(Detached{})
	{
		m_graphics_system = graphicsSystem;
		m_attached = true;
		acquire_shared_handles();
		if (m_graphics_system)
		{
			m_graphics_system->register_mesh3d(this);
		}
	}

	// Without the graphics system: no shared handle, not drawn (decode)
	MeshComponent3D_Bgfx::MeshComponent3D_Bgfx(Detached) :
		m_graphics_system(nullptr),
		m_vertexBuffer(BGFX_INVALID_HANDLE),
		m_indexBuffer(BGFX_INVALID_HANDLE),
		m_loaded(false),
		m_skinned(false),
		m_boundsMin{ 0.0f, 0.0f, 0.0f },
		m_boundsMax{ 0.0f, 0.0f, 0.0f },
		m_scene(nullptr),
		m_ticksPerSecond(1.0),
		m_timelineEndTicks(0.0),
		m_currentAnimation(-1),
		m_animationLoop(true),
		m_animationElapsedTicks(0.0),
		m_position{ 0.0f, 0.0f, 0.0f },
		m_rotation{ 0.0f, 0.0f, 0.0f },
		m_scale(1.0f),
		m_transform{ 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f }
	{
		// once, also when models decode on several threads
		static const bool layoutInitialized = []()
		{
			Vertex::init();
			SkinnedVertex::init();
			return true;
		}();
		(void)layoutInitialized;
		m_attached = false;
	}

	MeshComponent3D_Bgfx::~MeshComponent3D_Bgfx()
	{
		auto releaseResources = [this]()
		{
			if (m_graphics_system)
			{
				m_graphics_system->unregister_mesh3d(this);
			}
			release_resources();
			if (m_attached) release_shared_handles();
		};

		if (m_graphics_system)
		{
			auto state = m_graphics_system->get_state();
			if (state == SystemState::Running || state == SystemState::Paused)
			{
				auto task = m_graphics_system->execute_block(releaseResources);
				if (task)
				{
					task->wait();
				}
				return;
			}
		}

		releaseResources();
	}

	void MeshComponent3D_Bgfx::share_from(const MeshComponent3D_Bgfx& source)
	{
		release_resources();
		m_ownsResources = false;
		m_vertexBuffer = source.m_vertexBuffer;
		m_indexBuffer = source.m_indexBuffer;
		m_shadowVertexBuffer = source.m_shadowVertexBuffer;
		m_shadowIndexBuffer = source.m_shadowIndexBuffer;
		m_shadowIndexCount = source.m_shadowIndexCount;
		m_geometry = source.m_geometry;
		m_subMeshes = source.m_subMeshes;
		for (auto& sub : m_subMeshes)
		{
			sub.ownsTexture = false;
			sub.ownsExtraTextures = false;
		}
		m_pbr = source.m_pbr;
		std::copy(source.m_boundsMin, source.m_boundsMin + 3, m_boundsMin);
		std::copy(source.m_boundsMax, source.m_boundsMax + 3, m_boundsMax);
		m_skinned = source.m_skinned;
		m_loaded = source.m_loaded;
		m_hasTransparent = source.m_hasTransparent;
		if (m_skinned)
		{
			// The skeleton and the clips are read from the source's scene (animating only reads it); the pose is this one's
			m_scene = source.m_scene;
			m_palette = source.m_palette;
			m_nodesByName = source.m_nodesByName;
			m_channelsByNode = source.m_channelsByNode;
			m_clipChannels = source.m_clipChannels;
			m_activeChannels = nullptr;
			m_ticksPerSecond = source.m_ticksPerSecond;
			m_timelineEndTicks = source.m_timelineEndTicks;
			m_animations = source.m_animations;
			m_motionRoot = source.m_motionRoot;
			update(0.0f); // bind pose, so the palette exists before the first frame
		}
	}

	void MeshComponent3D_Bgfx::release_resources()
	{
		{
			std::lock_guard<std::mutex> lock(m_watchedMutex);
			m_watched.clear(); // their nodes belong to the scene that goes away
		}
		if (bgfx::isValid(m_paintMask))
		{
			release_texture(m_paintMask);
			m_paintMask = BGFX_INVALID_HANDLE;
		}
		for (auto& sub : m_subMeshes)
		{
			if (m_ownsResources && sub.ownsTexture && bgfx::isValid(sub.texture))
			{
				release_texture(sub.texture);
			}
			if (m_ownsResources && sub.ownsExtraTextures)
			{
				if (bgfx::isValid(sub.normalTexture)) release_texture(sub.normalTexture);
				if (bgfx::isValid(sub.ormTexture)) release_texture(sub.ormTexture);
			}
		}
		m_subMeshes.clear();
		m_geometry.reset();
		m_pbr = false;
		m_hasTransparent = false;

		if (m_ownsResources)
		{
			if (bgfx::isValid(m_vertexBuffer)) bgfx::destroy(m_vertexBuffer);
			if (bgfx::isValid(m_indexBuffer)) bgfx::destroy(m_indexBuffer);
			if (bgfx::isValid(m_shadowVertexBuffer)) bgfx::destroy(m_shadowVertexBuffer);
			if (bgfx::isValid(m_shadowIndexBuffer)) bgfx::destroy(m_shadowIndexBuffer);
		}
		m_ownsResources = true;
		m_vertexBuffer = BGFX_INVALID_HANDLE;
		m_indexBuffer = BGFX_INVALID_HANDLE;
		m_shadowVertexBuffer = BGFX_INVALID_HANDLE;
		m_shadowIndexBuffer = BGFX_INVALID_HANDLE;
		m_shadowIndexCount = 0;

		m_palette.clear();
		m_nodesByName.clear();
		m_nodeGlobal.clear();
		m_paletteMatrices.clear();
		m_channelsByNode.clear();
		m_animations.clear();
		m_scene = nullptr;
		m_importer.reset();
		m_motionRoot = nullptr;
		m_currentAnimation = -1;
		m_skinned = false;
		m_loaded = false;
	}

	std::string MeshComponent3D_Bgfx::take_load_stats()
	{
		char text[256];
		std::snprintf(text, sizeof(text), "%d models in %.0f ms: import %.0f, %d textures %.0f, shadow casters %.0f", s_stats.models,
			s_stats.total, s_stats.import, s_stats.textures_decoded, s_stats.textures, s_stats.shadow);
		s_stats = LoadStats();
		return text;
	}

	bool MeshComponent3D_Bgfx::load_model(const std::string& path)
	{
		return load_model(path, false);
	}

	std::unique_ptr<MeshComponent3D_Bgfx> MeshComponent3D_Bgfx::decode(const std::string& path)
	{
		std::unique_ptr<MeshComponent3D_Bgfx> model(new MeshComponent3D_Bgfx(Detached{}));
		return model->load_model(path, true) ? std::move(model) : nullptr;
	}

	bgfx::TextureHandle MeshComponent3D_Bgfx::request_texture(size_t sub, int slot, const std::string& path, const uint8_t* bytes, size_t size, bool& ok)
	{
		if (!m_pending)
		{
			const bgfx::TextureHandle handle = path.empty() ? load_texture_memory(bytes, size) : load_texture_file(path);
			ok = bgfx::isValid(handle);
			return handle;
		}
		PendingUpload::Texture request;
		request.sub = sub;
		request.slot = slot;
		request.path = path;
		if (path.empty())
		{
			request.image = parse_image(bytes, size);
			if (!request.image)
			{
				ok = false;
				return BGFX_INVALID_HANDLE;
			}
		}
		else if (!texture_cached(path))
		{
			request.shared = decode_shared(path);
			if (!request.shared || !request.shared->image)
			{
				ok = false;
				return BGFX_INVALID_HANDLE;
			}
		}
		m_pending->textures.push_back(request);
		ok = true;
		return BGFX_INVALID_HANDLE;   // made by upload()
	}

	bool MeshComponent3D_Bgfx::upload()
	{
		if (!m_pending)
		{
			return m_loaded;
		}
		if (!m_attached)
		{
			acquire_shared_handles();
			m_attached = true;
		}
		PendingUpload& pending = *m_pending;
		for (auto& request : pending.textures)
		{
			bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
			if (request.path.empty())
			{
				handle = create_texture(request.image);
				request.image = nullptr;   // freed with its texture made
			}
			else
			{
				handle = cache_texture(request.path, request.shared.get());
			}
			SubMesh& sub = m_subMeshes[request.sub];
			(request.slot == 0 ? sub.texture : request.slot == 1 ? sub.normalTexture : sub.ormTexture) = handle;
		}
		for (auto& sub : m_subMeshes)
		{
			if (!sub.ownsTexture || !bgfx::isValid(sub.texture))
			{
				sub.texture = s_whiteTexture;
				sub.ownsTexture = false;
			}
		}
		if (m_skinned)
		{
			m_vertexBuffer = bgfx::createVertexBuffer(
				bgfx::copy(pending.skinnedVertices.data(), static_cast<uint32_t>(pending.skinnedVertices.size() * sizeof(SkinnedVertex))),
				SkinnedVertex::ms_layout);
		}
		else
		{
			m_vertexBuffer = bgfx::createVertexBuffer(
				bgfx::copy(pending.staticVertices.data(), static_cast<uint32_t>(pending.staticVertices.size() * sizeof(Vertex))),
				Vertex::ms_layout);
		}
		m_indexBuffer = bgfx::createIndexBuffer(
			bgfx::copy(pending.indices.data(), static_cast<uint32_t>(pending.indices.size() * sizeof(uint32_t))),
			BGFX_BUFFER_INDEX32);
		if (!pending.shadowIndices.empty() && pending.shadowVertices.empty())
		{
			// a skinned model's: indices over its own vertices
			m_shadowIndexBuffer = bgfx::createIndexBuffer(bgfx::copy(pending.shadowIndices.data(), static_cast<uint32_t>(pending.shadowIndices.size() * sizeof(uint32_t))), BGFX_BUFFER_INDEX32);
		}
		else if (!pending.shadowIndices.empty())
		{
			if (s_positionLayout.getStride() == 0)
			{
				s_positionLayout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
			}
			m_shadowVertexBuffer = bgfx::createVertexBuffer(bgfx::copy(pending.shadowVertices.data(), static_cast<uint32_t>(pending.shadowVertices.size() * sizeof(float))), s_positionLayout);
			m_shadowIndexBuffer = bgfx::createIndexBuffer(bgfx::copy(pending.shadowIndices.data(), static_cast<uint32_t>(pending.shadowIndices.size() * sizeof(uint32_t))), BGFX_BUFFER_INDEX32);
		}
		m_pending.reset();
		m_loaded = bgfx::isValid(m_vertexBuffer) && bgfx::isValid(m_indexBuffer);
		return m_loaded;
	}

	bool MeshComponent3D_Bgfx::load_model(const std::string& path, bool deferGpu)
	{
		release_resources();
		m_pending = deferGpu ? std::make_unique<PendingUpload>() : nullptr;

		// glTF models come with their V axis up (OpenGL), while their PNG textures are stored top-down: V is
		// flipped for them. The level's Collada models use bottom-up TGA files and keep their V as is.
		std::string extension = fs::path(path).extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		const bool flipV = extension == ".gltf" || extension == ".glb";

		StatTimer totalTimer(s_stats.total);
		count_stat(s_stats.models);
		m_importer = std::make_unique<Assimp::Importer>();
		m_importer->SetIOHandler(new ResourceIOSystem());   // the importer owns it
		{
			StatTimer importTimer(s_stats.import);
			m_scene = m_importer->ReadFile(path,
				aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights
				| aiProcess_CalcTangentSpace);
		}
		if (!m_scene || !m_scene->mRootNode)
		{
			LOG_ERROR("[MeshComponent3D_Bgfx] Unable to load model '%s': %s", path.c_str(), m_importer->GetErrorString());
			release_resources();
			return false;
		}

		const fs::path modelDir = fs::path(path).parent_path();

		// Node lookup and the root transform inverse used by skinning
		std::function<void(const aiNode*)> indexNodes = [&](const aiNode* node)
		{
			m_nodesByName[node->mName.C_Str()] = node;
			for (unsigned int i = 0; i < node->mNumChildren; ++i)
			{
				indexNodes(node->mChildren[i]);
			}
		};
		indexNodes(m_scene->mRootNode);

		m_skinned = false;
		for (unsigned int mi = 0; mi < m_scene->mNumMeshes; ++mi)
		{
			if (m_scene->mMeshes[mi]->HasBones())
			{
				m_skinned = true;
				break;
			}
		}

		// Palette layout: one entry per bone (in first-seen order), then one per unskinned mesh node
		std::unordered_map<std::string, uint32_t> boneIndex;
		if (m_skinned)
		{
			for (unsigned int mi = 0; mi < m_scene->mNumMeshes; ++mi)
			{
				const aiMesh* mesh = m_scene->mMeshes[mi];
				for (unsigned int bi = 0; bi < mesh->mNumBones; ++bi)
				{
					const aiBone* bone = mesh->mBones[bi];
					const std::string name = bone->mName.C_Str();
					if (boneIndex.count(name))
					{
						continue;
					}
					auto nodeIt = m_nodesByName.find(name);
					boneIndex[name] = static_cast<uint32_t>(m_palette.size());
					m_palette.push_back({ nodeIt != m_nodesByName.end() ? nodeIt->second : nullptr, bone->mOffsetMatrix });
				}
			}
		}

		std::vector<Vertex> staticVertices;
		std::vector<SkinnedVertex> skinnedVertices;
		uint32_t vertexCount = 0;
		// Indices grouped by material, so every material becomes one submesh.
		std::vector<std::vector<uint32_t>> indicesByMaterial(m_scene->mNumMaterials);

		// Walks the node hierarchy. Static meshes get their node transform baked into the vertices;
		// skinned models keep bind-pose vertices and the palette moves them.
		std::function<void(const aiNode*, const aiMatrix4x4&)> collect = [&](const aiNode* node, const aiMatrix4x4& parent)
		{
			const aiMatrix4x4 global = parent * node->mTransformation;
			const aiMatrix3x3 normalMatrix(global);
			const bool mirrored = global.Determinant() < 0.0f;

			for (unsigned int mi = 0; mi < node->mNumMeshes; ++mi)
			{
				const aiMesh* mesh = m_scene->mMeshes[node->mMeshes[mi]];
				const uint32_t base = vertexCount;

				if (!m_skinned)
				{
					for (unsigned int vi = 0; vi < mesh->mNumVertices; ++vi)
					{
						const aiVector3D pos = global * mesh->mVertices[vi];
						aiVector3D normal = mesh->HasNormals() ? normalMatrix * mesh->mNormals[vi] : aiVector3D(0.0f, 0.0f, 1.0f);
						normal.Normalize();
						const aiVector3D uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][vi] : aiVector3D(0.0f, 0.0f, 0.0f);

						Vertex v;
						v.x = pos.x; v.y = pos.y; v.z = pos.z;
						v.nx = normal.x; v.ny = normal.y; v.nz = normal.z;
						v.u = uv.x; v.v = flipV ? 1.0f - uv.y : uv.y;
						aiVector3D tangent = mesh->HasTangentsAndBitangents() ? normalMatrix * mesh->mTangents[vi] : aiVector3D(1.0f, 0.0f, 0.0f);
						tangent.Normalize();
						v.tx = tangent.x; v.ty = tangent.y; v.tz = tangent.z;
						const aiVector3D uv1 = mesh->HasTextureCoords(1) ? mesh->mTextureCoords[1][vi] : aiVector3D(0.0f, flipV ? 1.0f : 0.0f, 0.0f);
						v.u1 = uv1.x; v.v1 = flipV ? 1.0f - uv1.y : uv1.y;
						staticVertices.push_back(v);
					}
				}
				else
				{
					// Per-vertex influences: up to four (bone, weight) pairs
					std::vector<std::array<std::pair<uint32_t, float>, 4>> influences(mesh->mNumVertices);
					for (auto& vi : influences)
					{
						vi.fill({ 0u, 0.0f });
					}

					if (mesh->HasBones())
					{
						for (unsigned int bi = 0; bi < mesh->mNumBones; ++bi)
						{
							const aiBone* bone = mesh->mBones[bi];
							const uint32_t paletteIndex = boneIndex[bone->mName.C_Str()];
							for (unsigned int wi = 0; wi < bone->mNumWeights; ++wi)
							{
								const aiVertexWeight& w = bone->mWeights[wi];
								auto& slots = influences[w.mVertexId];
								// Use the free slot, or replace the weakest one
								auto weakest = std::min_element(slots.begin(), slots.end(),
									[](const std::pair<uint32_t, float>& a, const std::pair<uint32_t, float>& b) { return a.second < b.second; });
								if (w.mWeight > weakest->second)
								{
									*weakest = { paletteIndex, w.mWeight };
								}
							}
						}
					}
					else
					{
						// Unskinned mesh inside a skinned model: follows its node through a palette entry of its own
						const uint32_t nodeEntry = static_cast<uint32_t>(m_palette.size());
						m_palette.push_back({ node, aiMatrix4x4() });
						for (auto& slots : influences)
						{
							slots[0] = { nodeEntry, 1.0f };
						}
					}

					for (unsigned int vi = 0; vi < mesh->mNumVertices; ++vi)
					{
						auto& slots = influences[vi];
						float total = 0.0f;
						for (const auto& s : slots) total += s.second;
						if (total <= 0.0f)
						{
							slots[0] = { 0u, 1.0f };
							total = 1.0f;
						}

						const aiVector3D pos = mesh->mVertices[vi];
						aiVector3D normal = mesh->HasNormals() ? mesh->mNormals[vi] : aiVector3D(0.0f, 0.0f, 1.0f);
						normal.Normalize();
						const aiVector3D uv = mesh->HasTextureCoords(0) ? mesh->mTextureCoords[0][vi] : aiVector3D(0.0f, 0.0f, 0.0f);

						SkinnedVertex v;
						v.x = pos.x; v.y = pos.y; v.z = pos.z;
						v.nx = normal.x; v.ny = normal.y; v.nz = normal.z;
						v.u = uv.x; v.v = flipV ? 1.0f - uv.y : uv.y;
						for (int s = 0; s < 4; ++s)
						{
							v.boneIndex[s] = static_cast<float>(slots[s].first);
							v.boneWeight[s] = slots[s].second / total;
						}
						skinnedVertices.push_back(v);
					}
				}
				vertexCount += mesh->mNumVertices;

				auto& bucket = indicesByMaterial[mesh->mMaterialIndex];
				for (unsigned int fi = 0; fi < mesh->mNumFaces; ++fi)
				{
					const aiFace& face = mesh->mFaces[fi];
					if (face.mNumIndices != 3)
					{
						continue;
					}
					// A mirrored transform flips the winding, so swap two indices to keep faces front-facing.
					// Skinned vertices are not transformed here, so their winding is kept as exported.
					const bool flip = !m_skinned && mirrored;
					bucket.push_back(base + face.mIndices[0]);
					bucket.push_back(base + face.mIndices[flip ? 2 : 1]);
					bucket.push_back(base + face.mIndices[flip ? 1 : 2]);
				}
			}

			for (unsigned int ci = 0; ci < node->mNumChildren; ++ci)
			{
				collect(node->mChildren[ci], global);
			}
		};
		collect(m_scene->mRootNode, aiMatrix4x4());

		if (m_palette.size() > kMaxPaletteEntries)
		{
			LOG_ERROR("[MeshComponent3D_Bgfx] Model '%s' has %zu bones, more than the %zu supported", path.c_str(), m_palette.size(), kMaxPaletteEntries);
			release_resources();
			return false;
		}

		std::vector<uint32_t> indices;
		std::vector<std::string> subMaterialNames; // one per submesh, for the material overrides
		for (unsigned int mi = 0; mi < m_scene->mNumMaterials; ++mi)
		{
			const auto& bucket = indicesByMaterial[mi];
			if (bucket.empty())
			{
				continue;
			}
			aiString materialName;
			m_scene->mMaterials[mi]->Get(AI_MATKEY_NAME, materialName);
			subMaterialNames.push_back(materialName.C_Str());

			const size_t subIndex = m_subMeshes.size();
			SubMesh sub{};
			sub.indexStart = static_cast<uint32_t>(indices.size());
			sub.indexCount = static_cast<uint32_t>(bucket.size());
			sub.texture = s_whiteTexture;
			sub.ownsTexture = false;

			aiString texPath;
			if (m_scene->mMaterials[mi]->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
			{
				// A texture inside the model file (glTF, "*0"): its compressed bytes decode like a file
				const aiTexture* embedded = m_scene->GetEmbeddedTexture(texPath.C_Str());
				const std::string resolved = embedded ? std::string() : resolve_texture_path(modelDir, texPath.C_Str());
				if (embedded)
				{
					bool ok = false;
					bgfx::TextureHandle tex = BGFX_INVALID_HANDLE;
					if (embedded->mHeight == 0) // compressed image (PNG): mWidth is its size in bytes
					{
						tex = request_texture(subIndex, 0, std::string(), reinterpret_cast<const uint8_t*>(embedded->pcData), embedded->mWidth, ok);
					}
					if (ok)
					{
						sub.texture = tex;
						sub.ownsTexture = true;
					}
					else
					{
						LOG_WARNING("[MeshComponent3D_Bgfx] Unable to decode the texture inside '%s', using white", path.c_str());
					}
				}
				else if (!resolved.empty())
				{
					bool ok = false;
					bgfx::TextureHandle tex = request_texture(subIndex, 0, resolved, nullptr, 0, ok);
					if (ok)
					{
						sub.texture = tex;
						sub.ownsTexture = true;
						sub.textureName = fs::path(resolved).stem().string();   // the atlas a paint mask is for (set_paint)
					}
					else
					{
						LOG_WARNING("[MeshComponent3D_Bgfx] Unable to decode texture '%s', using white", resolved.c_str());
					}
				}
				else
				{
					LOG_WARNING("[MeshComponent3D_Bgfx] Texture '%s' not found next to '%s', using white", texPath.C_Str(), path.c_str());
				}
			}

			// glTF: the normal map and the ORM texture (static models draw with them), and how the material blends
			if (!m_skinned)
			{
				auto load_extra = [&](aiTextureType type, int slot, bool& ok) -> bgfx::TextureHandle
				{
					ok = false;
					aiString reference;
					if (m_scene->mMaterials[mi]->GetTexture(type, 0, &reference) != AI_SUCCESS)
					{
						return BGFX_INVALID_HANDLE;
					}
					if (const aiTexture* embedded = m_scene->GetEmbeddedTexture(reference.C_Str()))
					{
						if (embedded->mHeight != 0) return BGFX_INVALID_HANDLE;
						return request_texture(subIndex, slot, std::string(), reinterpret_cast<const uint8_t*>(embedded->pcData), embedded->mWidth, ok);
					}
					const std::string resolved = resolve_texture_path(modelDir, reference.C_Str());
					if (resolved.empty()) return BGFX_INVALID_HANDLE;
						return request_texture(subIndex, slot, resolved, nullptr, 0, ok);
				};
				bool normalOk = false, ormOk = false;
				sub.normalTexture = load_extra(aiTextureType_NORMALS, 1, normalOk);
				sub.ormTexture = load_extra(aiTextureType_UNKNOWN, 2, ormOk); // glTF metallic-roughness
				sub.ownsExtraTextures = true;
				if (normalOk) m_pbr = true;
			}
			if (flipV)
			{
				aiString mode;
				if (m_scene->mMaterials[mi]->Get(AI_MATKEY_GLTF_ALPHAMODE, mode) == AI_SUCCESS)
				{
					if (std::string(mode.C_Str()) == "BLEND") sub.blend = MaterialBlend::Alpha;
					else if (std::string(mode.C_Str()) == "MASK") sub.blend = MaterialBlend::AlphaTest;
				}
				int twoSided = 0; // leaves and other cards are seen from both sides
				if (m_scene->mMaterials[mi]->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS && twoSided != 0) sub.doubleSided = true;
			}

			// A glTF material's base color factor: its plain color with no texture, and (as glTF says) a multiplier of its
			// texture with one (a darker sand, a tinted glass)
			if (flipV)
			{
				aiColor4D color;
				if (m_scene->mMaterials[mi]->Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS
					|| m_scene->mMaterials[mi]->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
				{
					sub.tint[0] = color.r;
					sub.tint[1] = color.g;
					sub.tint[2] = color.b;
				}
			}

			indices.insert(indices.end(), bucket.begin(), bucket.end());
			m_subMeshes.push_back(sub);
		}
		load_material_overrides(path, subMaterialNames);
		// The materials that blend decide whether there is a transparent pass (the overrides file may not exist)
		for (const auto& sub : m_subMeshes)
		{
			if (sub.blend == MaterialBlend::Alpha || sub.blend == MaterialBlend::Additive || sub.blend == MaterialBlend::Multiply)
			{
				m_hasTransparent = true;
			}
		}

		if (vertexCount == 0 || indices.empty())
		{
			LOG_ERROR("[MeshComponent3D_Bgfx] Model '%s' has no triangles", path.c_str());
			release_resources();
			return false;
		}

		// Bounds of the bind pose (before the object transform)
		float minB[3] = { 0.0f, 0.0f, 0.0f };
		float maxB[3] = { 0.0f, 0.0f, 0.0f };
		bool first = true;
		auto extend = [&](float x, float y, float z)
		{
			const float p[3] = { x, y, z };
			for (int i = 0; i < 3; ++i)
			{
				if (first) { minB[i] = maxB[i] = p[i]; }
				else { minB[i] = std::min(minB[i], p[i]); maxB[i] = std::max(maxB[i], p[i]); }
			}
			first = false;
		};
		if (m_skinned)
		{
			for (const auto& v : skinnedVertices) extend(v.x, v.y, v.z);
		}
		else
		{
			for (const auto& v : staticVertices) extend(v.x, v.y, v.z);
		}
		std::copy(minB, minB + 3, m_boundsMin);
		std::copy(maxB, maxB + 3, m_boundsMax);

		if (!m_pending)
		{
			if (m_skinned)
			{
				m_vertexBuffer = bgfx::createVertexBuffer(
					bgfx::copy(skinnedVertices.data(), static_cast<uint32_t>(skinnedVertices.size() * sizeof(SkinnedVertex))),
					SkinnedVertex::ms_layout);
			}
			else
			{
				m_vertexBuffer = bgfx::createVertexBuffer(
					bgfx::copy(staticVertices.data(), static_cast<uint32_t>(staticVertices.size() * sizeof(Vertex))),
					Vertex::ms_layout);
			}
			m_indexBuffer = bgfx::createIndexBuffer(
				bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint32_t))),
				BGFX_BUFFER_INDEX32);
		}
		if (m_skinned)
		{
			build_skinned_shadow(skinnedVertices, indices, path);
		}
		else
		{
			build_shadow_mesh(staticVertices, indices, path);
			if (staticVertices.size() <= kKeptGeometry)
			{
				auto kept = std::make_shared<StaticGeometry>();
				kept->vertices = staticVertices;
				kept->indices = indices;
				m_geometry = std::move(kept);
			}
		}

		// One timeline for the whole file: exporters split a motion into several animation
		// entries (one per node, or per property), so every channel is kept under its node name.
		m_ticksPerSecond = 1.0;
		m_timelineEndTicks = 0.0;
		for (unsigned int ai = 0; ai < m_scene->mNumAnimations; ++ai)
		{
			const aiAnimation* anim = m_scene->mAnimations[ai];
			if (ai == 0 && anim->mTicksPerSecond > 0.0)
			{
				m_ticksPerSecond = anim->mTicksPerSecond;
			}
			m_timelineEndTicks = std::max(m_timelineEndTicks, anim->mDuration);
			for (unsigned int ci = 0; ci < anim->mNumChannels; ++ci)
			{
				const aiNodeAnim* channel = anim->mChannels[ci];
				m_channelsByNode[channel->mNodeName.C_Str()].push_back(channel);
			}
		}

		// glTF: every animation is a clip of its own, with its own channels
		if (lower_extension(path) == ".gltf" || lower_extension(path) == ".glb")
		{
			m_clipChannels.assign(m_scene->mNumAnimations, ChannelMap());
			for (unsigned int ai = 0; ai < m_scene->mNumAnimations; ++ai)
			{
				const aiAnimation* anim = m_scene->mAnimations[ai];
				for (unsigned int ci = 0; ci < anim->mNumChannels; ++ci)
				{
					const aiNodeAnim* channel = anim->mChannels[ci];
					m_clipChannels[ai][channel->mNodeName.C_Str()].push_back(channel);
				}
			}
		}

		load_clips(path);

		// Root of the motion: the top-most node (breadth first) whose position really changes.
		// Exporters may sample every channel, so a channel with many keys can still be constant.
		m_motionRoot = nullptr;
		{
			std::vector<const aiNode*> queue{ m_scene->mRootNode };
			for (size_t qi = 0; qi < queue.size() && !m_motionRoot; ++qi)
			{
				const aiNode* node = queue[qi];
				auto channels = m_channelsByNode.find(node->mName.C_Str());
				if (channels != m_channelsByNode.end())
				{
					for (const aiNodeAnim* channel : channels->second)
					{
						for (unsigned int k = 1; k < channel->mNumPositionKeys; ++k)
						{
							const aiVector3D d = channel->mPositionKeys[k].mValue - channel->mPositionKeys[0].mValue;
							if (d.SquareLength() > 1e-6f)
							{
								m_motionRoot = node;
								break;
							}
						}
						if (m_motionRoot) break;
					}
				}
				for (unsigned int ci = 0; ci < node->mNumChildren; ++ci)
				{
					queue.push_back(node->mChildren[ci]);
				}
			}
		}

		// (decoding, the model counts as loaded for the bind pose below; upload() makes it so)
		m_loaded = m_pending ? true : bgfx::isValid(m_vertexBuffer) && bgfx::isValid(m_indexBuffer);
		if (m_skinned)
		{
			update(0.0f); // bind pose, so the palette exists before the first frame

			// Bounds of the skinned bind pose, in the same frame the model is drawn in (the raw
			// vertices can sit in another frame: bind shape matrices, file up axis and unit)
			first = true;
			for (const auto& v : skinnedVertices)
			{
				float out[3] = { 0.0f, 0.0f, 0.0f };
				for (int s = 0; s < 4; ++s)
				{
					const float* m = &m_paletteMatrices[static_cast<size_t>(v.boneIndex[s]) * 16];
					const float w = v.boneWeight[s];
					out[0] += w * (m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12]);
					out[1] += w * (m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13]);
					out[2] += w * (m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14]);
				}
				extend(out[0], out[1], out[2]);
			}
			std::copy(minB, minB + 3, m_boundsMin);
			std::copy(maxB, maxB + 3, m_boundsMax);
		}

		LOG_INFO("[MeshComponent3D_Bgfx] Loaded '%s': %u vertices, %zu triangles, %zu submeshes, %s, %zu palette entries, %zu animations, bounds (%.2f %.2f %.2f) .. (%.2f %.2f %.2f)",
			path.c_str(), vertexCount, indices.size() / 3, m_subMeshes.size(), m_skinned ? "skinned" : "static",
			m_palette.size(), m_animations.size(),
			minB[0], minB[1], minB[2], maxB[0], maxB[1], maxB[2]);
		if (m_pending)
		{
			m_pending->staticVertices = std::move(staticVertices);
			m_pending->skinnedVertices = std::move(skinnedVertices);
			m_pending->indices = std::move(indices);
			m_loaded = false;   // until upload()
			return true;
		}
		return m_loaded;
	}

	const std::string& MeshComponent3D_Bgfx::get_animation_name(int index) const
	{
		if (index < 0 || index >= static_cast<int>(m_animations.size()))
		{
			return s_emptyName;
		}
		return m_animations[index].name;
	}

	int MeshComponent3D_Bgfx::find_animation(const std::string& name) const
	{
		for (size_t i = 0; i < m_animations.size(); ++i)
		{
			if (m_animations[i].name == name)
			{
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	void MeshComponent3D_Bgfx::play_animation(int index, bool loop, double startSeconds)
	{
		if (index < 0 || index >= static_cast<int>(m_animations.size()))
		{
			LOG_WARNING("[MeshComponent3D_Bgfx] Animation index %d out of range (%zu clips)", index, m_animations.size());
			return;
		}
		m_currentAnimation = index;
		m_animationLoop = loop;
		m_animationElapsedTicks = 0.0;
		m_settledClip = -1;
		const double length = m_animations[index].endTicks - m_animations[index].startTicks;
		if (startSeconds > 0.0 && length > 0.0 && m_ticksPerSecond > 0.0)
		{
			const double ticks = startSeconds * m_ticksPerSecond;
			m_animationElapsedTicks = loop ? std::fmod(ticks, length) : std::min(ticks, length);
		}
		m_activeChannels = m_clipChannels.empty() ? nullptr : &m_clipChannels[static_cast<size_t>(index)];
	}

	void MeshComponent3D_Bgfx::play_overlay(int index, const std::string& fromNode, bool loop, float speed)
	{
		if (index < 0 || index >= static_cast<int>(m_animations.size()))
		{
			return;
		}
		auto node = m_nodesByName.find(fromNode);
		if (node == m_nodesByName.end())
		{
			LOG_WARNING("[MeshComponent3D_Bgfx] play_overlay: no node '%s'", fromNode.c_str());
			return;
		}
		if (index != m_overlay || m_overlayEnding || !loop)
		{
			m_overlayElapsedTicks = 0.0;
		}
		m_overlay = index;
		m_overlayLoop = loop;
		m_overlayEnding = false;
		m_overlaySpeed = speed;
		m_overlayRoot = node->second;
	}

	void MeshComponent3D_Bgfx::stop_overlay()
	{
		if (m_overlay >= 0)
		{
			m_overlayEnding = true; // fades out
		}
	}

	double MeshComponent3D_Bgfx::get_clip_length_seconds(int index) const
	{
		if (index < 0 || index >= static_cast<int>(m_animations.size()) || m_ticksPerSecond <= 0.0)
		{
			return 0.0;
		}
		return (m_animations[index].endTicks - m_animations[index].startTicks) / m_ticksPerSecond;
	}

	void MeshComponent3D_Bgfx::add_clip_mark(int index, double seconds, const std::string& name)
	{
		if (index < 0 || index >= static_cast<int>(m_animations.size()))
		{
			return;
		}
		auto& marks = m_clipMarks[index];
		marks.push_back({ seconds * m_ticksPerSecond, name });
		std::stable_sort(marks.begin(), marks.end(), [](const AnimationMark& a, const AnimationMark& b) { return a.at < b.at; });
	}

	void MeshComponent3D_Bgfx::update(float dt_seconds, bool pose)
	{
		if (!m_loaded || !m_skinned || hidden())
		{
			return;
		}

		// The overlay clip runs on its own clock and blends in and out over a moment
		if (m_overlay >= 0)
		{
			const AnimationClip& clip = m_animations[m_overlay];
			const double length = clip.endTicks - clip.startTicks;
			m_overlayElapsedTicks += dt_seconds * m_overlaySpeed * m_ticksPerSecond;
			if (!m_overlayLoop && m_overlayElapsedTicks >= length)
			{
				m_overlayEnding = true;
			}
			const double offset = length <= 0.0 ? 0.0
				: (m_overlayLoop ? std::fmod(m_overlayElapsedTicks, length) : std::min(m_overlayElapsedTicks, length));
			m_overlayTimeTicks = clip.startTicks + offset;
			const float blend = std::min(1.0f, dt_seconds / kOverlayBlendSeconds);
			m_overlayWeight = m_overlayEnding ? std::max(0.0f, m_overlayWeight - blend) : std::min(1.0f, m_overlayWeight + blend);
			if (m_overlayEnding && m_overlayWeight <= 0.0f)
			{
				m_overlay = -1;
			}
		}

		bool animated = false;
		bool settled = false;   // a clip played once, at its end
		double timeTicks = 0.0;
		int markedClip = -1;
		std::vector<std::string> passedMarks;
		if (m_currentAnimation >= 0)
		{
			const AnimationClip& clip = m_animations[m_currentAnimation];
			const double length = clip.endTicks - clip.startTicks;
			const double before = m_animationElapsedTicks;
			m_animationElapsedTicks += dt_seconds * m_animationSpeed * m_ticksPerSecond;
			if (onClipMark)
			{
				auto marks = m_clipMarks.find(m_currentAnimation);
				if (marks != m_clipMarks.end())
				{
					// reported once the pose is done: the handler may add marks or play another clip
					markedClip = m_currentAnimation;
					for (const AnimationMark* mark : marks_passed(marks->second, before, m_animationElapsedTicks, length, m_animationLoop))
					{
						passedMarks.push_back(mark->name);
					}
				}
			}

			if (length > 0.0)
			{
				// a negative speed plays the clip backwards
				double offset = m_animationLoop
					? std::fmod(m_animationElapsedTicks, length)
					: std::clamp(m_animationElapsedTicks, 0.0, length);
				if (offset < 0.0) offset += length;
				timeTicks = clip.startTicks + offset;
				settled = !m_animationLoop && (m_animationSpeed >= 0.0f ? m_animationElapsedTicks >= length : m_animationElapsedTicks <= 0.0) && m_overlay < 0;
			}
			else
			{
				timeTicks = clip.startTicks;
			}
			animated = true;
		}

		// Out of the camera's view and of the shadow's: the clip's clock and its marks go on, the pose is worked out when it
		// is seen again (once there is a palette to draw with)
		const bool unseen = !m_paletteMatrices.empty() && s_cullFrusta[0].valid && outside_frustum(0) && !casts_into_view();
		if (unseen)
		{
			for (const auto& mark : passedMarks) onClipMark(markedClip, mark);
			return;
		}
		// Not posed this frame, or its clip settled at its end and already posed there: last bones where the model is now
		// (the palette carries its place)
		if ((!pose || (settled && m_settledClip == m_currentAnimation)) && !m_paletteMatrices.empty())
		{
			set_transform();
			build_palette();
			for (const auto& mark : passedMarks) onClipMark(markedClip, mark);
			return;
		}
		set_transform();
		// Nodes without a channel keep their bind-pose transform
		compute_node_transforms(m_scene->mRootNode, aiMatrix4x4(), animated, timeTicks);
		m_settledClip = settled ? m_currentAnimation : -1;
		apply_reach();
		build_palette();
		for (const auto& mark : passedMarks)
		{
			onClipMark(markedClip, mark);
		}
	}

	void MeshComponent3D_Bgfx::set_reach(const std::string& upper, const std::string& lower, const std::string& end,
		const std::string& anchor, const float offset[3], float weight)
	{
		if (weight <= 0.0f || !m_scene || !m_scene->mRootNode)
		{
			m_reach.weight = 0.0f;
			return;
		}
		if (!m_reach.upper || m_reach.upper->mName.C_Str() != upper || m_reach.anchor->mName.C_Str() != anchor)
		{
			m_reach.upper = m_scene->mRootNode->FindNode(upper.c_str());
			m_reach.lower = m_scene->mRootNode->FindNode(lower.c_str());
			m_reach.end = m_scene->mRootNode->FindNode(end.c_str());
			m_reach.anchor = m_scene->mRootNode->FindNode(anchor.c_str());
		}
		m_reach.offset = aiVector3D(offset[0], offset[1], offset[2]);
		m_reach.weight = (m_reach.upper && m_reach.lower && m_reach.end && m_reach.anchor) ? std::min(1.0f, weight) : 0.0f;
	}

	void MeshComponent3D_Bgfx::transform_subtree(const aiNode* node, const aiMatrix4x4& delta)
	{
		auto it = m_nodeGlobal.find(node);
		if (it != m_nodeGlobal.end()) it->second = delta * it->second;
		for (unsigned i = 0; i < node->mNumChildren; ++i) transform_subtree(node->mChildren[i], delta);
	}

	namespace
	{
		aiVector3D origin_of(const aiMatrix4x4& m) { return aiVector3D(m.a4, m.b4, m.c4); }

		// The rotation that turns direction 'from' into 'to' (both normalized), about the point 'pivot'
		aiMatrix4x4 turn_about(const aiVector3D& pivot, aiVector3D from, aiVector3D to)
		{
			from.Normalize();
			to.Normalize();
			aiMatrix4x4 rotation;
			const float cosine = std::clamp(from * to, -1.0f, 1.0f);
			aiVector3D axis = from ^ to;
			if (axis.Length() < 1.0e-6f) return aiMatrix4x4();   // already there (or exactly opposite: left alone)
			axis.Normalize();
			aiMatrix4x4::Rotation(std::acos(cosine), axis, rotation);
			aiMatrix4x4 there, back;
			aiMatrix4x4::Translation(pivot, there);
			aiMatrix4x4::Translation(-pivot, back);
			return there * rotation * back;
		}
	}

	// Two-bone IK on the pose just computed (model space): the shoulder turns so the elbow lands where the arm's two lengths
	// reach the target, in the plane of the clip's bend; then the elbow turns so the end lands on it
	void MeshComponent3D_Bgfx::apply_reach()
	{
		Reach& r = m_reach;
		if (r.weight <= 0.0f) return;
		auto global = [this](const aiNode* node) { auto it = m_nodeGlobal.find(node); return it != m_nodeGlobal.end() ? it->second : aiMatrix4x4(); };
		const aiVector3D a = origin_of(global(r.upper)), b = origin_of(global(r.lower)), c = origin_of(global(r.end));
		const aiVector3D goal = global(r.anchor) * r.offset;
		const aiVector3D target = c + r.weight * (goal - c);
		const float upperLength = (b - a).Length(), lowerLength = (c - b).Length();
		aiVector3D toTarget = target - a;
		const float reach = std::clamp(toTarget.Length(), std::fabs(upperLength - lowerLength) + 1.0e-4f, upperLength + lowerLength - 1.0e-4f);
		if (toTarget.Length() < 1.0e-6f || upperLength < 1.0e-6f || lowerLength < 1.0e-6f) return;
		const aiVector3D u = toTarget.Normalize();
		// the bend: the clip's elbow off the shoulder-target line (or any side when the arm is straight)
		aiVector3D side = (b - a) - ((b - a) * u) * u;
		if (side.Length() < 1.0e-5f) side = std::fabs(u.y) < 0.9f ? (u ^ aiVector3D(0.0f, 1.0f, 0.0f)) : (u ^ aiVector3D(1.0f, 0.0f, 0.0f));
		side.Normalize();
		const float cosShoulder = std::clamp((upperLength * upperLength + reach * reach - lowerLength * lowerLength) / (2.0f * upperLength * reach), -1.0f, 1.0f);
		const aiVector3D elbow = a + upperLength * (cosShoulder * u + std::sqrt(1.0f - cosShoulder * cosShoulder) * side);
		const aiMatrix4x4 shoulderTurn = turn_about(a, b - a, elbow - a);
		transform_subtree(r.upper, shoulderTurn);
		const aiVector3D movedEnd = origin_of(global(r.end));
		transform_subtree(r.lower, turn_about(elbow, movedEnd - elbow, (a + reach * u) - elbow));
	}

	double MeshComponent3D_Bgfx::get_animation_length_seconds() const
	{
		if (m_currentAnimation < 0 || m_ticksPerSecond <= 0.0)
		{
			return 0.0;
		}
		const AnimationClip& clip = m_animations[m_currentAnimation];
		return (clip.endTicks - clip.startTicks) / m_ticksPerSecond;
	}

	// Local transform of a node at a given time. Each component (position, rotation, scale) comes
	// from the channel of the node with the most keys of it; components without a channel keep the
	// value of the node's bind-pose transform. Under the overlay's root node, the overlay clip is
	// blended over the main one by its weight.
	void MeshComponent3D_Bgfx::compute_node_transforms(const aiNode* node, const aiMatrix4x4& parent, bool animated, double timeTicks, bool inOverlay)
	{
		aiMatrix4x4 local = node->mTransformation;
		inOverlay = inOverlay || (m_overlay >= 0 && node == m_overlayRoot);

		aiVector3D bindScale;
		aiQuaternion bindRot;
		aiVector3D bindPos;
		node->mTransformation.Decompose(bindScale, bindRot, bindPos);

		// Samples this node in a set of channels; false when the set has no channel for it
		auto sample = [&](const ChannelMap& channels, double ticks, aiVector3D& pos, aiQuaternion& rot, aiVector3D& scl)
		{
			auto channelsIt = channels.find(node->mName.C_Str());
			if (channelsIt == channels.end())
			{
				return false;
			}
			const aiNodeAnim* posChannel = nullptr;
			const aiNodeAnim* rotChannel = nullptr;
			const aiNodeAnim* sclChannel = nullptr;
			// A file can split one node's motion over several channels, and each channel also carries
			// the components it does not animate as a constant (one or few keys). The channel with the
			// most keys of a component is the one that animates it.
			for (const aiNodeAnim* channel : channelsIt->second)
			{
				if (channel->mNumPositionKeys > 0 && (!posChannel || channel->mNumPositionKeys > posChannel->mNumPositionKeys)) posChannel = channel;
				if (channel->mNumRotationKeys > 0 && (!rotChannel || channel->mNumRotationKeys > rotChannel->mNumRotationKeys)) rotChannel = channel;
				if (channel->mNumScalingKeys > 0 && (!sclChannel || channel->mNumScalingKeys > sclChannel->mNumScalingKeys)) sclChannel = channel;
			}
			pos = posChannel ? sample_vector(posChannel->mPositionKeys, posChannel->mNumPositionKeys, ticks, bindPos) : bindPos;
			rot = rotChannel ? sample_rotation(rotChannel->mRotationKeys, rotChannel->mNumRotationKeys, ticks, bindRot) : bindRot;
			scl = sclChannel ? sample_vector(sclChannel->mScalingKeys, sclChannel->mNumScalingKeys, ticks, bindScale) : bindScale;
			return true;
		};

		aiVector3D pos = bindPos, scl = bindScale;
		aiQuaternion rot = bindRot;
		bool posed = false;
		if (animated && sample(m_activeChannels ? *m_activeChannels : m_channelsByNode, timeTicks, pos, rot, scl))
		{
			posed = true;
			if (m_inPlace && node == m_motionRoot)
			{
				// Keep only the motion along the scene's up axis (Y), seen from the parent's frame
				aiMatrix3x3 parentRotation(parent);
				parentRotation.Inverse();
				aiVector3D up = parentRotation * aiVector3D(0.0f, 1.0f, 0.0f);
				up.Normalize();
				const aiVector3D delta = pos - bindPos;
				pos = bindPos + up * (delta * up);
			}
		}
		if (inOverlay && m_overlayWeight > 0.0f)
		{
			const ChannelMap& overlayChannels = m_clipChannels.empty() ? m_channelsByNode : m_clipChannels[static_cast<size_t>(m_overlay)];
			aiVector3D overlayPos, overlayScl;
			aiQuaternion overlayRot;
			if (sample(overlayChannels, m_overlayTimeTicks, overlayPos, overlayRot, overlayScl))
			{
				const float w = m_overlayWeight;
				aiQuaternion blended;
				aiQuaternion::Interpolate(blended, rot, overlayRot, w);
				rot = blended.Normalize();
				pos = pos + (overlayPos - pos) * w;
				scl = scl + (overlayScl - scl) * w;
				posed = true;
			}
		}
		if (posed)
		{
			aiMatrix4x4 T;
			aiMatrix4x4::Translation(pos, T);
			aiMatrix4x4 R(rot.GetMatrix());
			aiMatrix4x4 S;
			aiMatrix4x4::Scaling(scl, S);
			local = T * R * S;
		}

		const aiMatrix4x4 global = parent * local;
		m_nodeGlobal[node] = global;
		for (unsigned int i = 0; i < node->mNumChildren; ++i)
		{
			compute_node_transforms(node->mChildren[i], global, animated, timeTicks, inOverlay);
		}
	}

	// Named clips from the <animation_clip> entries of the file (start and end in seconds).
	// A file without them gets one clip covering the whole timeline.
	void MeshComponent3D_Bgfx::load_material_overrides(const std::string& modelPath, const std::vector<std::string>& materialNames)
	{
		m_hasTransparent = false;
		pugi::xml_document doc;
		const auto overrides = ResourceMgr.read_bytes(modelPath + ".materials.xml");
		if (!overrides || !doc.load_buffer(overrides->data(), overrides->size()))
		{
			return;
		}

		for (pugi::xml_node material : doc.child("materials").children("material"))
		{
			const std::string name = material.attribute("name").value();
			const std::string blend = material.attribute("blend").value();
			MaterialBlend mode = MaterialBlend::Opaque;
			if (blend == "alphatest") mode = MaterialBlend::AlphaTest;
			else if (blend == "alpha") mode = MaterialBlend::Alpha;
			else if (blend == "additive") mode = MaterialBlend::Additive;
			else if (blend == "multiply") mode = MaterialBlend::Multiply;

			float tint[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			const char* tintText = material.attribute("tint").value();
			if (*tintText)
			{
				std::sscanf(tintText, "%f %f %f %f", &tint[0], &tint[1], &tint[2], &tint[3]);
			}

			bool matched = false;
			for (size_t i = 0; i < m_subMeshes.size() && i < materialNames.size(); ++i)
			{
				if (materialNames[i] != name)
				{
					continue;
				}
				matched = true;
				SubMesh& sub = m_subMeshes[i];
				// only what the entry says: the rest stays as the model has it (its color factor, its blend)
				if (*blend.c_str()) sub.blend = mode;
				if (material.attribute("doubleSided")) sub.doubleSided = material.attribute("doubleSided").as_bool(false);
				if (*tintText) std::copy(tint, tint + 4, sub.tint);
				const char* scrollText = material.attribute("scroll").value();
				if (*scrollText) std::sscanf(scrollText, "%f %f", &sub.scroll[0], &sub.scroll[1]);
				if (material.attribute("water")) sub.water = material.attribute("water").as_float(0.0f);
				const char* detileText = material.attribute("detile").value();
				if (*detileText) std::sscanf(detileText, "%f %f", &sub.detile[0], &sub.detile[1]);
			}
			if (!matched)
			{
				std::string known;
				for (const auto& materialName : materialNames) known += (known.empty() ? "" : ", ") + materialName;
				LOG_WARNING("[MeshComponent3D_Bgfx] '%s': override for material '%s' matches none of the model's (%s)", modelPath.c_str(), name.c_str(), known.c_str());
			}
		}

		for (const auto& sub : m_subMeshes)
		{
			if (sub.blend == MaterialBlend::Alpha || sub.blend == MaterialBlend::Additive || sub.blend == MaterialBlend::Multiply)
			{
				m_hasTransparent = true;
			}
		}
	}

	void MeshComponent3D_Bgfx::load_clips(const std::string& path)
	{
		m_animations.clear();

		// glTF has no clip list of its own: every animation of the scene is one clip, by its name
		const std::string extension = lower_extension(path);
		if (extension == ".gltf" || extension == ".glb")
		{
			for (unsigned int ai = 0; ai < m_scene->mNumAnimations; ++ai)
			{
				const aiAnimation* anim = m_scene->mAnimations[ai];
				std::string name = anim->mName.C_Str();
				if (name.empty())
				{
					name = "animation" + std::to_string(ai);
				}
				m_animations.push_back({ name, 0.0, anim->mDuration });
			}
			if (!m_animations.empty())
			{
				return;
			}
		}

		pugi::xml_document doc;
		const auto clips = ResourceMgr.read_bytes(path);
		if (clips && doc.load_buffer(clips->data(), clips->size()))
		{
			std::function<void(pugi::xml_node)> visit = [&](pugi::xml_node node)
			{
				for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling())
				{
					if (std::strcmp(child.name(), "animation_clip") == 0)
					{
						AnimationClip clip;
						clip.name = child.attribute("id").value();
						clip.startTicks = child.attribute("start").as_double() * m_ticksPerSecond;
						clip.endTicks = child.attribute("end").as_double() * m_ticksPerSecond;
						if (clip.endTicks > clip.startTicks)
						{
							m_animations.push_back(clip);
						}
					}
					visit(child);
				}
			};
			visit(doc);
		}

		if (m_animations.empty() && m_timelineEndTicks > 0.0)
		{
			m_animations.push_back({ "animation", 0.0, m_timelineEndTicks });
		}
	}

	void MeshComponent3D_Bgfx::watch_node(const std::string& name)
	{
		if (!m_scene || !m_scene->mRootNode)
		{
			return;
		}
		const aiNode* node = m_scene->mRootNode->FindNode(name.c_str());
		if (!node)
		{
			LOG_WARNING("[MeshComponent3D_Bgfx] no node '%s' to follow", name.c_str());
			return;
		}
		std::lock_guard<std::mutex> lock(m_watchedMutex);
		m_watched.push_back({ name, node });
	}

	bool MeshComponent3D_Bgfx::get_node_world(const std::string& name, float out[16]) const
	{
		std::lock_guard<std::mutex> lock(m_watchedMutex);
		for (const auto& watched : m_watched)
		{
			if (watched.name == name && watched.known)
			{
				std::copy(watched.world.begin(), watched.world.end(), out);
				return true;
			}
		}
		return false;
	}

	void MeshComponent3D_Bgfx::build_palette()
	{
		m_paletteMatrices.resize(m_palette.size() * 16);
		const aiMatrix4x4 object = read_column_major(m_transform);
		{
			std::lock_guard<std::mutex> lock(m_watchedMutex);
			for (auto& watched : m_watched)
			{
				auto it = m_nodeGlobal.find(watched.node);
				if (it != m_nodeGlobal.end())
				{
					write_column_major(object * it->second, watched.world.data());
					watched.known = true;
				}
			}
		}

		for (size_t i = 0; i < m_palette.size(); ++i)
		{
			aiMatrix4x4 nodeGlobal;
			if (m_palette[i].node)
			{
				auto it = m_nodeGlobal.find(m_palette[i].node);
				if (it != m_nodeGlobal.end())
				{
					nodeGlobal = it->second;
				}
			}

			// object * node(animated, root included) * inverse bind (offset). Keeping the root's
			// transform puts skinned models in the scene frame Assimp builds (Y up, meters), the same
			// frame static models are baked into, whatever the up axis or unit of the file.
			const aiMatrix4x4 final = object * nodeGlobal * m_palette[i].offset;
			write_column_major(final, &m_paletteMatrices[i * 16]);
		}
	}

	void MeshComponent3D_Bgfx::set_position(float x, float y, float z)
	{
		m_position[0] = x;
		m_position[1] = y;
		m_position[2] = z;
		mark_moved();
	}

	void MeshComponent3D_Bgfx::set_rotation(float pitch, float yaw, float roll)
	{
		m_rotation[0] = pitch;
		m_rotation[1] = yaw;
		m_rotation[2] = roll;
		mark_moved();
	}

	void MeshComponent3D_Bgfx::set_scale(float uniform)
	{
		m_scale = uniform;
		mark_moved();
	}

	void MeshComponent3D_Bgfx::mark_moved()
	{
		m_transformDirty = true;
		++m_moves;
		if (m_gridded) s_gridMoved = true;
	}

	bool MeshComponent3D_Bgfx::take_grid_moved()
	{
		return s_gridMoved.exchange(false);
	}

	float MeshComponent3D_Bgfx::shadow_reach()
	{
		return s_shadowReach;
	}

	bool MeshComponent3D_Bgfx::box_outside_frustum(int slot, const float center[3], const float extent[3])
	{
		const CullFrustum& frustum = s_cullFrusta[slot];
		if (!frustum.valid) return false;
		for (const auto& plane : frustum.planes)
		{
			const float distance = plane[0] * center[0] + plane[1] * center[1] + plane[2] * center[2] + plane[3];
			const float radius = std::fabs(plane[0]) * extent[0] + std::fabs(plane[1]) * extent[1] + std::fabs(plane[2]) * extent[2];
			if (distance + radius < 0.0f) return true;
		}
		return false;
	}

	void MeshComponent3D_Bgfx::get_bounds(float min[3], float max[3]) const
	{
		std::copy(m_boundsMin, m_boundsMin + 3, min);
		std::copy(m_boundsMax, m_boundsMax + 3, max);
	}

	void MeshComponent3D_Bgfx::set_matrix(const float* matrix16)
	{
		std::copy(matrix16, matrix16 + 16, m_matrix);
		m_hasMatrix = true;
		mark_moved();
	}

	void MeshComponent3D_Bgfx::attach_to(MeshComponent3D_Bgfx* parent, const std::string& node, const float local[16])
	{
		m_attachParent = parent;
		mark_moved();
		m_attachNode = node;
		std::copy(local, local + 16, m_attachLocal.begin());
		if (parent)
		{
			parent->watch_node(node);
		}
	}

	void MeshComponent3D_Bgfx::set_transform()
	{
		if (m_attachParent)
		{
			float bone[16];
			if (m_attachParent->get_node_world(m_attachNode, bone))
			{
				// transform = bone * local (column-major)
				for (int c = 0; c < 4; ++c)
					for (int r = 0; r < 4; ++r)
					{
						float sum = 0.0f;
						for (int k = 0; k < 4; ++k) sum += bone[k * 4 + r] * m_attachLocal[c * 4 + k];
						m_transform[c * 4 + r] = sum;
					}
			}
			else
			{
				std::fill(m_transform, m_transform + 16, 0.0f); // the bone is not placed yet: draw nothing
			}
			return;
		}
		if (!m_transformDirty) return;
		m_transformDirty = false;
		if (m_hasMatrix)
		{
			std::copy(m_matrix, m_matrix + 16, m_transform);
			return;
		}
		bx::mtxSRT(m_transform,
			m_scale, m_scale, m_scale,
			m_rotation[0], m_rotation[1], m_rotation[2],
			m_position[0], m_position[1], m_position[2]);
	}

	void MeshComponent3D_Bgfx::set_gi_state(bgfx::TextureHandle volume, const float origin[4], const float size[4])
	{
		s_giVolume = volume;
		std::copy(origin, origin + 4, s_giOrigin);
		std::copy(size, size + 4, s_giSize);
	}

	void MeshComponent3D_Bgfx::set_style(float gloss, float shininess)
	{
		s_style[0] = gloss;
		s_style[1] = shininess;
	}

	void MeshComponent3D_Bgfx::set_lightmap_state(bgfx::TextureHandle atlas, float shadowStrength)
	{
		s_lightmapAtlas = atlas;
		s_lightmapShadow = shadowStrength;
	}

	bgfx::TextureHandle MeshComponent3D_Bgfx::load_level_texture(const std::string& path)
	{
		const fs::path file(path);
		const std::string resolved = resolve_texture_path(file.parent_path(), file.filename().string());
		return resolved.empty() ? bgfx::TextureHandle(BGFX_INVALID_HANDLE) : load_texture_file(resolved);
	}

	bool MeshComponent3D_Bgfx::set_paint(const std::string& atlas, const std::string& mask, const float wall[3], const float roof[3])
	{
		const bool used = std::any_of(m_subMeshes.begin(), m_subMeshes.end(), [&atlas](const SubMesh& sub) { return sub.textureName == atlas; });
		if (!used) return false;
		const bgfx::TextureHandle texture = load_level_texture(mask);
		if (!bgfx::isValid(texture)) return false;
		if (bgfx::isValid(m_paintMask)) release_texture(m_paintMask);
		m_paintMask = texture;
		m_paintAtlas = atlas;
		std::copy(wall, wall + 3, m_paintWall);
		std::copy(roof, roof + 3, m_paintRoof);
		m_paintWall[3] = m_paintRoof[3] = 1.0f;
		return true;
	}

	void MeshComponent3D_Bgfx::release_level_texture(bgfx::TextureHandle texture)
	{
		if (bgfx::isValid(texture))
		{
			release_texture(texture);
		}
	}

	void MeshComponent3D_Bgfx::set_depth_program(bgfx::ProgramHandle program)
	{
		s_depthProgram = program;
	}

	void MeshComponent3D_Bgfx::set_shadow_state(bgfx::TextureHandle map, const float matrix[16], const float params[4])
	{
		s_shadowMap = map;
		std::copy(matrix, matrix + 16, s_shadowMatrix);
		std::copy(params, params + 4, s_shadowParams);
	}

	void MeshComponent3D_Bgfx::set_cull_frustum(int slot, const float m[16], bool homogeneousDepth)
	{
		// The planes are the rows of the matrix combined: row3 +- row0 (sides), row3 +- row1 (top and bottom), near, far
		auto row = [m](int r, float out[4]) { for (int i = 0; i < 4; ++i) out[i] = m[i * 4 + r]; };
		float r0[4], r1[4], r2[4], r3[4];
		row(0, r0); row(1, r1); row(2, r2); row(3, r3);
		CullFrustum& frustum = s_cullFrusta[slot];
		for (int i = 0; i < 4; ++i)
		{
			frustum.planes[0][i] = r3[i] + r0[i];
			frustum.planes[1][i] = r3[i] - r0[i];
			frustum.planes[2][i] = r3[i] + r1[i];
			frustum.planes[3][i] = r3[i] - r1[i];
			frustum.planes[4][i] = homogeneousDepth ? r3[i] + r2[i] : r2[i];
			frustum.planes[5][i] = r3[i] - r2[i];
		}
		frustum.valid = true;
	}

	std::unique_ptr<MeshComponent3D_Bgfx> MeshComponent3D_Bgfx::merge(BgfxGraphicsSystem* graphics, const std::vector<MeshComponent3D_Bgfx*>& copies)
	{
		if (copies.empty() || !copies.front()->m_geometry) return nullptr;
		const MeshComponent3D_Bgfx& first = *copies.front();
		const StaticGeometry& shape = *first.m_geometry;
		for (const auto* copy : copies)
		{
			if (!copy->m_loaded || copy->m_skinned || copy->m_geometry != first.m_geometry) return nullptr;
		}
		const size_t vertexCount = shape.vertices.size();
		std::vector<Vertex> vertices;
		vertices.reserve(vertexCount * copies.size());
		for (auto* copy : copies)
		{
			copy->set_transform();
			const float* t = copy->m_transform;   // column-major
			auto turn = [t](float x, float y, float z, float& ox, float& oy, float& oz)
			{
				ox = t[0] * x + t[4] * y + t[8] * z;
				oy = t[1] * x + t[5] * y + t[9] * z;
				oz = t[2] * x + t[6] * y + t[10] * z;
				const float length = std::sqrt(ox * ox + oy * oy + oz * oz);
				if (length > 1e-8f) { ox /= length; oy /= length; oz /= length; }
			};
			for (Vertex v : shape.vertices)
			{
				const float x = v.x, y = v.y, z = v.z;
				v.x = t[0] * x + t[4] * y + t[8] * z + t[12];
				v.y = t[1] * x + t[5] * y + t[9] * z + t[13];
				v.z = t[2] * x + t[6] * y + t[10] * z + t[14];
				turn(v.nx, v.ny, v.nz, v.nx, v.ny, v.nz);
				turn(v.tx, v.ty, v.tz, v.tx, v.ty, v.tz);
				vertices.push_back(v);
			}
		}
		// each material's triangles together, copy after copy: one draw per material
		std::vector<uint32_t> indices;
		indices.reserve(shape.indices.size() * copies.size());
		auto merged = std::make_unique<MeshComponent3D_Bgfx>(graphics);
		merged->m_subMeshes = first.m_subMeshes;
		for (auto& sub : merged->m_subMeshes)
		{
			const uint32_t start = sub.indexStart, count = sub.indexCount;
			sub.indexStart = static_cast<uint32_t>(indices.size());
			for (size_t c = 0; c < copies.size(); ++c)
			{
				const uint32_t base = static_cast<uint32_t>(c * vertexCount);
				for (uint32_t i = start; i < start + count; ++i) indices.push_back(base + shape.indices[i]);
			}
			sub.indexCount = static_cast<uint32_t>(indices.size()) - sub.indexStart;
			sub.ownsTexture = false;   // the model's: it outlives the merged one (the asset cache keeps it)
			sub.ownsExtraTextures = false;
		}
		merged->m_pbr = first.m_pbr;
		merged->m_hasTransparent = first.m_hasTransparent;
		float low[3] = { 1e30f, 1e30f, 1e30f }, high[3] = { -1e30f, -1e30f, -1e30f };
		for (const auto& v : vertices)
		{
			low[0] = std::min(low[0], v.x); low[1] = std::min(low[1], v.y); low[2] = std::min(low[2], v.z);
			high[0] = std::max(high[0], v.x); high[1] = std::max(high[1], v.y); high[2] = std::max(high[2], v.z);
		}
		std::copy(low, low + 3, merged->m_boundsMin);
		std::copy(high, high + 3, merged->m_boundsMax);
		merged->m_vertexBuffer = bgfx::createVertexBuffer(bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(Vertex))), Vertex::ms_layout);
		merged->m_indexBuffer = bgfx::createIndexBuffer(bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint32_t))), BGFX_BUFFER_INDEX32);
		merged->build_shadow_mesh(vertices, indices, "merged");
		merged->m_loaded = bgfx::isValid(merged->m_vertexBuffer) && bgfx::isValid(merged->m_indexBuffer);
		merged->set_position(0.0f, 0.0f, 0.0f);
		merged->set_scale(1.0f);
		return merged;
	}

	void MeshComponent3D_Bgfx::set_sort_view(const float view[16])
	{
		s_sortRow[0] = view[2];
		s_sortRow[1] = view[6];
		s_sortRow[2] = view[10];
		s_sortRow[3] = view[14];
		s_sortValid = true;
		// the eye: minus the view's translation turned back by its rotation (column-major)
		for (int i = 0; i < 3; ++i) s_cullEye[i] = -(view[i * 4] * view[12] + view[i * 4 + 1] * view[13] + view[i * 4 + 2] * view[14]);
	}

	void MeshComponent3D_Bgfx::set_draw_distance(float perMeter, float minimum)
	{
		s_drawPerMeter = perMeter;
		s_drawMinimum = minimum;
	}

	// In the sun's view, and its shadow can reach the camera's: its top's height over the ground times how far a shadow goes
	// sideways per meter (without a camera or that reach, whatever the sun sees)
	bool MeshComponent3D_Bgfx::casts_into_view()
	{
		if (!s_cullFrusta[1].valid || outside_frustum(1)) return false;
		if (s_shadowReach < 0.0f || !s_cullFrusta[0].valid) return true;
		float center[3], extent[3];
		get_world_box(center, extent);
		const float top = std::max(0.0f, center[1] + extent[1]);
		return !outside_frustum(0, top * s_shadowReach + 0.5f);
	}

	void MeshComponent3D_Bgfx::set_shadow_reach(float perMeter)
	{
		s_shadowReach = perMeter;
	}

	void MeshComponent3D_Bgfx::clear_cull_frustum(int slot)
	{
		s_cullFrusta[slot].valid = false;
	}

	// The model's box (moved by its transform) lies completely outside one plane of the frustum. A skinned model's pose
	// moves it out of the box of its bind pose (a body lying down, an arm reaching): its box is a cube as wide as the bind
	// pose's largest side, around its middle, a little larger.
	void MeshComponent3D_Bgfx::get_world_box(float center[3], float extent[3])
	{
		set_transform();
		const float* t = m_transform;
		const float cx = 0.5f * (m_boundsMin[0] + m_boundsMax[0]), cy = 0.5f * (m_boundsMin[1] + m_boundsMax[1]), cz = 0.5f * (m_boundsMin[2] + m_boundsMax[2]);
		const float ex = 0.5f * (m_boundsMax[0] - m_boundsMin[0]), ey = 0.5f * (m_boundsMax[1] - m_boundsMin[1]), ez = 0.5f * (m_boundsMax[2] - m_boundsMin[2]);
		for (int axis = 0; axis < 3; ++axis)
		{
			center[axis] = t[axis] * cx + t[4 + axis] * cy + t[8 + axis] * cz + t[12 + axis];
			extent[axis] = std::fabs(t[axis]) * ex + std::fabs(t[4 + axis]) * ey + std::fabs(t[8 + axis]) * ez;
		}
	}

	bool MeshComponent3D_Bgfx::outside_frustum(int slot, float margin)
	{
		const CullFrustum& frustum = s_cullFrusta[slot];
		if (!frustum.valid)
		{
			return false;
		}
		set_transform();
		const float* t = m_transform;
		const float cx = 0.5f * (m_boundsMin[0] + m_boundsMax[0]), cy = 0.5f * (m_boundsMin[1] + m_boundsMax[1]), cz = 0.5f * (m_boundsMin[2] + m_boundsMax[2]);
		float ex = 0.5f * (m_boundsMax[0] - m_boundsMin[0]), ey = 0.5f * (m_boundsMax[1] - m_boundsMin[1]), ez = 0.5f * (m_boundsMax[2] - m_boundsMin[2]);
		if (m_skinned)
		{
			ex = ey = ez = 1.15f * std::max(ex, std::max(ey, ez));
		}
		const float grow = margin / std::max(0.0001f, m_scale);   // the extents are in the model's units
		ex += grow;
		ey += grow;
		ez += grow;
		float center[3], extent[3];
		for (int axis = 0; axis < 3; ++axis)
		{
			center[axis] = t[axis] * cx + t[4 + axis] * cy + t[8 + axis] * cz + t[12 + axis];
			extent[axis] = std::fabs(t[axis]) * ex + std::fabs(t[4 + axis]) * ey + std::fabs(t[8 + axis]) * ez;
		}
		// the camera's own draw distance by size: a model farther than its size times s_drawPerMeter (never nearer than
		// s_drawMinimum) is a few pixels; left out (a camera low over the floor looks across the whole level)
		if (slot == 0 && s_drawPerMeter > 0.0f)
		{
			const float size = 2.0f * std::max(extent[0], std::max(extent[1], extent[2]));
			const float dx = center[0] - s_cullEye[0], dy = center[1] - s_cullEye[1], dz = center[2] - s_cullEye[2];
			const float reach = std::max(s_drawMinimum, size * s_drawPerMeter);
			if (dx * dx + dy * dy + dz * dz > reach * reach) return true;
		}
		for (const auto& plane : frustum.planes)
		{
			const float distance = plane[0] * center[0] + plane[1] * center[1] + plane[2] * center[2] + plane[3];
			const float radius = std::fabs(plane[0]) * extent[0] + std::fabs(plane[1]) * extent[1] + std::fabs(plane[2]) * extent[2];
			if (distance + radius < 0.0f)
			{
				return true;
			}
		}
		return false;
	}

	bool MeshComponent3D_Bgfx::drawn_in_depth_view()
	{
		if (hidden() || !m_loaded || (m_boundsMax[1] - m_boundsMin[1]) * m_scale < 0.35f && !m_skinned || outside_frustum(1))
		{
			return false;
		}
		return casts_into_view();
	}

	void MeshComponent3D_Bgfx::render_depth(bgfx::ViewId viewId, bgfx::ProgramHandle program)
	{
		if (!bgfx::isValid(program) || !drawn_in_depth_view()) return;
		uint32_t transformCache = 0;
		uint16_t transformCount = 1;
		if (m_skinned)
		{
			transformCount = static_cast<uint16_t>(m_palette.size());
			transformCache = bgfx::setTransform(m_paletteMatrices.data(), transformCount);
		}
		else
		{
			set_transform();
			transformCache = bgfx::setTransform(m_transform);
		}
		if (m_shadowIndexCount > 0)
		{
			bgfx::setTransform(transformCache, transformCount);
			bgfx::setVertexBuffer(0, m_skinned ? m_vertexBuffer : m_shadowVertexBuffer);
			bgfx::setIndexBuffer(m_shadowIndexBuffer);
			bgfx::setState(BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS);
			bgfx::submit(viewId, program);
			s_passStats[0].triangles += m_shadowIndexCount / 3;
			++s_passStats[0].models;
			return;
		}
		for (const auto& sub : m_subMeshes)
		{
			if (sub.blend != MaterialBlend::Opaque && sub.blend != MaterialBlend::AlphaTest)
			{
				continue;
			}
			bgfx::setTransform(transformCache, transformCount);
			bgfx::setVertexBuffer(0, m_vertexBuffer);
			bgfx::setIndexBuffer(m_indexBuffer, sub.indexStart, sub.indexCount);
			bgfx::setState(BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS);
			bgfx::submit(viewId, program);
			s_passStats[0].triangles += sub.indexCount / 3;
		}
		++s_passStats[0].models;
	}

	void MeshComponent3D_Bgfx::take_pass_stats(PassStats out[3])
	{
		for (int i = 0; i < 3; ++i)
		{
			out[i] = s_passStats[i];
			s_passStats[i] = PassStats{};
		}
	}

	// What a submesh's draw needs besides its geometry, transform and state: its textures and the uniforms (shared by the
	// draw of one copy and the instanced draw of many)
	void MeshComponent3D_Bgfx::bind_material(const SubMesh& sub, bool faded, const float material[4]) const
	{
		bgfx::setTexture(0, s_texColorSampler, sub.texture);
		const float tint[4] = { sub.tint[0] * m_color[0], sub.tint[1] * m_color[1], sub.tint[2] * m_color[2],
			sub.tint[3] * (faded ? m_opacity : 1.0f) };
		bgfx::setUniform(s_tintUniform, tint);
		bgfx::setUniform(s_materialUniform, material);
		{
			// a material that flows (water): its texture slides at its speed, on a clock of its own (wrapped, so the
			// offset stays small)
			static const auto clockStart = std::chrono::steady_clock::now();
			const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - clockStart).count();
			// z: water (its strength), w: the clock for its ripples (wrapped every 10 minutes: whole turns of their sines)
			const float scroll[4] = { static_cast<float>(std::fmod(seconds * sub.scroll[0], 1.0)),
				static_cast<float>(std::fmod(seconds * sub.scroll[1], 1.0)), sub.water, static_cast<float>(std::fmod(seconds, 600.0)) };
			bgfx::setUniform(s_uvScrollUniform, scroll);
			const float ground[4] = { sub.detile[0], sub.detile[1], 0.0f, 0.0f };
			bgfx::setUniform(s_groundUniform, ground);
		}
		if (m_pbr)
		{
			bgfx::setTexture(1, s_normalSampler, bgfx::isValid(sub.normalTexture) ? sub.normalTexture : s_flatNormalTexture);
			bgfx::setTexture(2, s_ormSampler, bgfx::isValid(sub.ormTexture) ? sub.ormTexture : s_plainOrmTexture);
		}
		bgfx::setUniform(s_giOriginUniform, s_giOrigin);
		if (s_giOrigin[3] > 0.0f)
		{
			bgfx::setUniform(s_giSizeUniform, s_giSize);
			bgfx::setTexture(5, s_giSampler, s_giVolume);
		}
		else if (bgfx::isValid(s_dummyVolume))
		{
			bgfx::setTexture(5, s_giSampler, s_dummyVolume);
		}
		// u_lightmap: xyz the place in the atlas, w: 0 no lightmap, 1 + how much the baked shadow darkens
		const bool lightmapped = m_lightmapRect[0] > 0.0f && bgfx::isValid(s_lightmapAtlas);
		const float lightmap[4] = { m_lightmapRect[0], m_lightmapRect[1], m_lightmapRect[2], lightmapped ? 1.0f + s_lightmapShadow : 0.0f };
		bgfx::setUniform(s_lightmapUniform, lightmap);
		// the dents are drawn in the model's own units: per meter, times its scale
		const float style[4] = { s_style[0], s_style[1], m_clay, m_clayDents * std::max(m_scale, 0.0001f) };
		bgfx::setUniform(s_styleUniform, style);
		bgfx::setTexture(6, s_lightmapSampler, lightmapped ? s_lightmapAtlas : s_whiteTexture);
		// its paint, on the materials of the atlas the mask lies over (a sampler is always bound: an unbound one draws
		// nothing on some GLES drivers)
		const bool paintHere = bgfx::isValid(m_paintMask) && sub.textureName == m_paintAtlas;
		static const float unpainted[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		bgfx::setTexture(7, s_paintSampler, paintHere ? m_paintMask : s_whiteTexture);
		bgfx::setUniform(s_paintWallUniform, paintHere ? m_paintWall : unpainted);
		bgfx::setUniform(s_paintRoofUniform, paintHere ? m_paintRoof : unpainted);
		bgfx::setUniform(s_shadowParamsUniform, s_shadowParams);
		if (s_shadowParams[0] > 0.0f)
		{
			bgfx::setUniform(s_shadowMtxUniform, s_shadowMatrix);
			bgfx::setTexture(4, s_shadowSampler, s_shadowMap);
		}
		else if (bgfx::isValid(s_dummyShadow))
		{
			bgfx::setTexture(4, s_shadowSampler, s_dummyShadow);
		}
	}

	bool MeshComponent3D_Bgfx::render_instances(bgfx::ViewId viewId, bgfx::ProgramHandle program,
		const std::vector<MeshComponent3D_Bgfx*>& copies, bool depth)
	{
		if (copies.empty() || !bgfx::isValid(program)) return true;
		const uint16_t stride = 64;   // a 4x4 matrix: i_data0..3, its columns
		const uint32_t count = static_cast<uint32_t>(copies.size());
		if (bgfx::getAvailInstanceDataBuffer(count, stride) < count) return false;
		bgfx::InstanceDataBuffer instances;
		bgfx::allocInstanceDataBuffer(&instances, count, stride);
		float* out = reinterpret_cast<float*>(instances.data);
		for (MeshComponent3D_Bgfx* copy : copies)
		{
			copy->set_transform();
			std::copy(copy->m_transform, copy->m_transform + 16, out);
			out += 16;
		}
		const MeshComponent3D_Bgfx& first = *copies.front();
		const int pass = depth ? 0 : 1;
		if (depth && first.m_shadowIndexCount > 0)
		{
			bgfx::setInstanceDataBuffer(&instances);
			bgfx::setVertexBuffer(0, first.m_shadowVertexBuffer);
			bgfx::setIndexBuffer(first.m_shadowIndexBuffer);
			bgfx::setState(BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS);
			bgfx::submit(viewId, program);
			s_passStats[0].triangles += first.m_shadowIndexCount / 3 * count;
			s_passStats[0].models += count;
			return true;
		}
		static const bool noCull = std::getenv("PAKAL_NO_CULL") != nullptr;
		for (const auto& sub : first.m_subMeshes)
		{
			if (sub.blend != MaterialBlend::Opaque && sub.blend != MaterialBlend::AlphaTest) continue;
			bgfx::setInstanceDataBuffer(&instances);
			bgfx::setVertexBuffer(0, first.m_vertexBuffer);
			bgfx::setIndexBuffer(first.m_indexBuffer, sub.indexStart, sub.indexCount);
			if (depth)
			{
				bgfx::setState(BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS);
			}
			else
			{
				const float material[4] = { sub.blend == MaterialBlend::AlphaTest ? 0.5f : 0.0f, 0.0f, 0.0f, 0.0f };
				first.bind_material(sub, false, material);
				bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS
					| BGFX_STATE_MSAA | (sub.doubleSided || noCull ? 0ull : BGFX_STATE_CULL_CW));
			}
			bgfx::submit(viewId, program);
			s_passStats[pass].triangles += sub.indexCount / 3 * count;
		}
		s_passStats[pass].models += count;
		return true;
	}

	void MeshComponent3D_Bgfx::render(bgfx::ViewId viewId, bgfx::ProgramHandle program, bool transparentPass)
	{
		const bool faded = m_opacity < 0.995f && !m_fadeDither;   // blended in the transparent pass
		const bool dithered = m_opacity < 0.995f && m_fadeDither;  // solid, with a share of its pixels left out
		if (hidden() || !m_loaded || !bgfx::isValid(program) || (transparentPass && !m_hasTransparent && !faded)
			|| outside_frustum(0))
		{
			return;
		}

		// The transform goes into bgfx's per-frame cache once; every submesh points at it.
		// submit() discards the draw state, so transform and vertex buffer are set for each submesh.
		uint32_t transformCache = 0;
		uint16_t transformCount = 1;
		if (m_skinned)
		{
			// Palette already includes the object transform
			transformCount = static_cast<uint16_t>(m_palette.size());
			transformCache = bgfx::setTransform(m_paletteMatrices.data(), transformCount);
		}
		else
		{
			set_transform();
			transformCache = bgfx::setTransform(m_transform);
		}
		// Front faces are counter-clockwise (right-handed camera); PAKAL_NO_CULL is a debug override
		static const bool noCull = std::getenv("PAKAL_NO_CULL") != nullptr;
		const uint64_t cullState = noCull ? 0ull : BGFX_STATE_CULL_CW;
		// Its distance in front of the camera, in centimeters: the main view draws the nearest first
		uint32_t sortDepth = 0;
		if (s_sortValid && !transparentPass)
		{
			float center[3], extent[3];
			get_world_box(center, extent);
			const float along = s_sortRow[0] * center[0] + s_sortRow[1] * center[1] + s_sortRow[2] * center[2] + s_sortRow[3];
			sortDepth = static_cast<uint32_t>(std::clamp(std::fabs(along) * 100.0f, 0.0f, 4.0e9f));
		}

		// A faded model draws its solid materials blended, without writing depth: a depth pass of its own first would hide
		// whatever blended thing is behind it (a faded tree, particles, glass)
		for (const auto& sub : m_subMeshes)
		{
			// A faded instance draws its solid materials blended too, in the transparent pass
			const bool fadedSolid = faded && (sub.blend == MaterialBlend::Opaque || sub.blend == MaterialBlend::AlphaTest);
			const bool blended = fadedSolid || sub.blend == MaterialBlend::Alpha || sub.blend == MaterialBlend::Additive
				|| sub.blend == MaterialBlend::Multiply;
			if (blended != transparentPass)
			{
				continue;
			}

			uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA
				| (sub.doubleSided ? 0ull : cullState);
			// u_material: x = alpha test reference (0 = off), y = keep the texture alpha, z = unlit, w = dithered opacity (0 = off)
			float material[4] = { 0.0f, 0.0f, 0.0f, dithered ? std::max(0.05f, m_opacity) : 0.0f };
			switch (sub.blend)
			{
			case MaterialBlend::Opaque:
			case MaterialBlend::AlphaTest:
				if (fadedSolid)
				{
					state |= BGFX_STATE_BLEND_ALPHA; // no depth write: what is behind shows through
					material[0] = sub.blend == MaterialBlend::AlphaTest ? 0.5f : 0.0f;
					material[1] = 1.0f;
					break;
				}
				state |= BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z;
				material[0] = sub.blend == MaterialBlend::AlphaTest ? 0.5f : 0.0f;
				break;
			case MaterialBlend::Alpha:
				state |= BGFX_STATE_BLEND_ALPHA;
				material[1] = 1.0f;
				break;
			case MaterialBlend::Additive:
				state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);
				material[1] = 1.0f;
				material[2] = 1.0f;
				break;
			case MaterialBlend::Multiply:
				state |= BGFX_STATE_BLEND_MULTIPLY;
				material[2] = 1.0f;
				break;
			}

			bgfx::setTransform(transformCache, transformCount);
			bgfx::setVertexBuffer(0, m_vertexBuffer);
			bgfx::setIndexBuffer(m_indexBuffer, sub.indexStart, sub.indexCount);
			bind_material(sub, faded, material);
			bgfx::setState(state);
			bgfx::submit(viewId, program, sortDepth);
			s_passStats[transparentPass ? 2 : 1].triangles += sub.indexCount / 3;
		}
		++s_passStats[transparentPass ? 2 : 1].models;
	}
}
