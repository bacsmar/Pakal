#include "ResourceManager.h"
#include "DirectorySourceImpl.h"
#include "SingletonHolder.h"


#include <algorithm>
#include <memory>
#include <fstream>
#include <filesystem>

namespace Pakal
{
	ResourceManager& ResourceManager::instance() 
	{
		static SingletonHolder<ResourceManager> sh;
		return *sh.get();
	}

	void ResourceManager::initialize()
	{
		ASSERT(m_memory_streams.empty());
		if (m_root_path.empty())
		{
			m_root_path = ".";
		}
		// Register default stdlib-based source factories.
		// These have no external dependencies and are always available. A platform may have mounted its own sources
		// before the engine runs (Android: the APK's assets); they stay.
		if (!has_source_factory<DirectorySource>())
		{
			register_source<DirectorySource>([]() -> DirectorySource* { return new DirectorySourceImpl(); });
		}
	}

	void ResourceManager::terminate()
	{
		forget_known();
		m_sources.clear();
		m_factories.clear();
		m_memory_streams.clear();
	}

	std::string ResourceManager::logical_path(const std::string& path) const
	{
		namespace fs = std::filesystem;
		std::string name = fs::path(path).lexically_normal().generic_string();
		std::string root = fs::path(m_root_path).lexically_normal().generic_string();
		if (!root.empty() && root != "." && root.back() != '/') root += '/';
		if (!root.empty() && root != "." && name.compare(0, root.size(), root) == 0) name = name.substr(root.size());
		while (name.compare(0, 2, "./") == 0) name = name.substr(2);
		return name;
	}

	void ResourceManager::set_root_path(const std::string& root_path)
	{
		m_root_path = root_path.empty() ? "." : root_path;
		forget_known();
		LOG_INFO("[ResourceManager] root path set to: %s", m_root_path.c_str());
	}

	std::string ResourceManager::resolve_file_path(const std::string& resource_path) const
	{
		namespace fs = std::filesystem;

		if (resource_path.empty())
		{
			return resource_path;
		}

		fs::path requested(resource_path);
		if (requested.is_absolute() && fs::exists(requested))
		{
			return requested.string();
		}

		if (!m_root_path.empty())
		{
			fs::path rooted = fs::path(m_root_path) / requested;
			if (fs::exists(rooted))
			{
				return rooted.string();
			}
		}

		return requested.string();
	}

	void ResourceManager::remove_source(ISource* source) 
	{
		mutex_guard guard(m_sources_mutex);

		auto sourcePos = 
			std::find_if(m_sources.begin(), 
						 m_sources.end(), 
						[source](const SharedPtr<ISource>& i) { return i.get() == source; });

		m_sources.erase(sourcePos);
		forget_known();
	}

	SharedPtr<std::istream> ResourceManager::open_read_resource(const Path& resourcePath, bool inMemory)
	{
		namespace fs = std::filesystem;

		//try to open it from cache
		if (inMemory)
		{
			mutex_guard lock(m_memory_streams_mutex);

			WeakPtr<memory_istream> cached_stream;
			if (map_utils::try_get(m_memory_streams, resourcePath, cached_stream))
			{
				if (auto cachedStream = cached_stream.lock())
				{
					LOG_INFO("[ResourceManager] %s loaded from cache", resourcePath.c_str());
					return  memory_istream::from_buffer(cachedStream);
				}
				else
				{
					map_utils::erase_if(m_memory_streams, [](const std::pair<Path, WeakPtr<memory_istream>>& str) { return str.second.expired(); });
				}
			}
		}


		SharedPtr<std::istream> stream;

		auto try_open_file = [](const fs::path& candidate) -> SharedPtr<std::istream>
		{
			auto localStream = std::make_shared<std::ifstream>(candidate.string().c_str(), std::ios_base::in | std::ios_base::binary);
			if (localStream->is_open())
			{
				return std::static_pointer_cast<std::istream>(localStream);
			}
			return nullptr;
		};

		//try to open it from current dir
		const std::string requested_path_str = resourcePath.c_str();
		fs::path requested_path(requested_path_str);
		if (requested_path.is_absolute())
		{
			stream = try_open_file(requested_path);
		}

		if (!stream && !m_root_path.empty())
		{
			stream = try_open_file(fs::path(m_root_path) / requested_path);
		}

		if (!stream)
		{
			stream = try_open_file(requested_path);
		}

		if (!stream) //try to open it from sources
		{
			m_sources_mutex.lock();
				std::vector<SharedPtr<ISource>> tempSources = m_sources;
			m_sources_mutex.unlock();

			for (auto& source : tempSources)
				if ( (stream = source->open_resource(resourcePath)) )
					break;
		}


		if (stream)
		{
			if (inMemory)
			{
				SharedPtr<memory_istream> memoryStream = memory_istream::from_istream(stream.get());

				mutex_guard lock(m_memory_streams_mutex);
				m_memory_streams[resourcePath] = memoryStream;

				LOG_INFO("[ResourceManager] %s loaded in memory", resourcePath.c_str());
				return memoryStream;
			}
			else
			{
				LOG_INFO("[ResourceManager] %s loaded", resourcePath.c_str());
				return stream;
			}
		}

		LOG_ERROR("[ResourceManager] %s could not be loaded", resourcePath.c_str());
		return nullptr;
	}

	SharedPtr<std::istream> ResourceManager::open_from_any(const std::string& path)
	{
		namespace fs = std::filesystem;
		if (path.empty()) return nullptr;
		auto try_open = [](const fs::path& candidate) -> SharedPtr<std::istream>
		{
			auto stream = std::make_shared<std::ifstream>(candidate, std::ios_base::in | std::ios_base::binary);
			return stream->is_open() ? std::static_pointer_cast<std::istream>(stream) : nullptr;
		};
		const fs::path requested(path);
		SharedPtr<std::istream> stream = try_open(requested);
		if (!stream && !requested.is_absolute() && !m_root_path.empty() && m_root_path != ".")
		{
			stream = try_open(fs::path(m_root_path) / requested);
		}
		if (!stream)
		{
			std::vector<SharedPtr<ISource>> sources;
			{
				mutex_guard guard(m_sources_mutex);
				sources = m_sources;
			}
			for (auto& source : sources)
			{
				if ((stream = source->open_resource(Path(path)))) break;
			}
		}
		return stream;
	}

	SharedPtr<const std::vector<uint8_t>> ResourceManager::read_bytes(const std::string& path)
	{
		const std::string key = std::filesystem::path(path).lexically_normal().generic_string();
		{
			mutex_guard guard(m_bytes_mutex);
			auto found = m_bytes.find(key);
			if (found != m_bytes.end())
			{
				if (auto held = found->second.lock()) return held;
				m_bytes.erase(found);
			}
		}
		{
			// a file known not to be anywhere is not looked for again (an optional one: a model's .materials.xml)
			mutex_guard guard(m_known_mutex);
			auto known = m_known.find(key);
			if (known != m_known.end() && !known->second) return nullptr;
		}
		SharedPtr<std::istream> stream = open_from_any(key);
		{
			mutex_guard guard(m_known_mutex);
			m_known[key] = stream != nullptr;
		}
		if (!stream) return nullptr;
		auto bytes = std::make_shared<std::vector<uint8_t>>((std::istreambuf_iterator<char>(*stream)), std::istreambuf_iterator<char>());
		SharedPtr<const std::vector<uint8_t>> shared = bytes;
		mutex_guard guard(m_bytes_mutex);
		m_bytes[key] = shared;
		return shared;
	}

	std::string ResourceManager::read_text(const std::string& path, bool* found)
	{
		const auto bytes = read_bytes(path);
		if (found) *found = bytes != nullptr;
		return bytes ? std::string(bytes->begin(), bytes->end()) : std::string();
	}

	void ResourceManager::forget_known()
	{
		mutex_guard guard(m_known_mutex);
		m_known.clear();
	}

	bool ResourceManager::exists(const std::string& path)
	{
		if (path.empty()) return false;
		const std::string key = std::filesystem::path(path).lexically_normal().generic_string();
		{
			mutex_guard guard(m_known_mutex);
			auto found = m_known.find(key);
			if (found != m_known.end()) return found->second;
		}
		const bool there = find_anywhere(key);
		mutex_guard guard(m_known_mutex);
		m_known[key] = there;
		return there;
	}

	bool ResourceManager::find_anywhere(const std::string& path)
	{
		namespace fs = std::filesystem;
		std::error_code error;
		const fs::path requested(path);
		if (fs::is_regular_file(requested, error)) return true;
		if (!requested.is_absolute() && !m_root_path.empty() && fs::is_regular_file(fs::path(m_root_path) / requested, error)) return true;
		{
			mutex_guard guard(m_bytes_mutex);
			auto found = m_bytes.find(requested.lexically_normal().generic_string());
			if (found != m_bytes.end() && !found->second.expired()) return true;
		}
		std::vector<SharedPtr<ISource>> sources;
		{
			mutex_guard guard(m_sources_mutex);
			sources = m_sources;
		}
		for (auto& source : sources)
		{
			if (source->has_resource(Path(path))) return true;
		}
		return false;
	}

	SharedPtr<std::ostream> ResourceManager::open_write_resource(const Path& resourcePath)
	{
		forget_known();   // a file that was not there may be now
		auto stream = std::make_shared<std::ofstream>(resourcePath.c_str());

		return stream->is_open() ? stream : nullptr;
	}

}