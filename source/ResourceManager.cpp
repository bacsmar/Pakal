#include "ResourceManager.h"

#include "SingletonHolder.h"


#include <algorithm>
#include <memory>
#include <fstream>

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
	}

	void ResourceManager::terminate()
	{
		m_sources.clear();
		m_factories.clear();
		m_memory_streams.clear();
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

		//try to open it from current dir
		auto localStream = std::make_shared<std::ifstream>(resourcePath.c_str(),std::ios_base::in | std::ios_base::binary);
		if (localStream->is_open())
		{			
			stream =  std::static_pointer_cast<std::istream>(localStream);
		}
		else //try to open it from sources
		{
			m_sources_mutex.lock();
				std::vector<SharedPtr<ISource>> tempSources = m_sources;
			m_sources_mutex.unlock();

			for (auto& source : m_sources)
				if (stream = source->open_resource(resourcePath))
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