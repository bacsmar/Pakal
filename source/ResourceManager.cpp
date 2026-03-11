#include "ResourceManager.h"

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
		ASSERT(m_sources.empty());
		ASSERT(m_factories.empty());
		ASSERT(m_memory_streams.empty());
		if (m_root_path.empty())
		{
			m_root_path = ".";
		}
	}

	void ResourceManager::terminate()
	{
		m_sources.clear();
		m_factories.clear();
		m_memory_streams.clear();
	}

	void ResourceManager::set_root_path(const std::string& root_path)
	{
		m_root_path = root_path.empty() ? "." : root_path;
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

	SharedPtr<std::ostream> ResourceManager::open_write_resource(const Path& resourcePath)
	{
		auto stream = std::make_shared<std::ofstream>(resourcePath.c_str());

		return stream->is_open() ? stream : nullptr;
	}

}