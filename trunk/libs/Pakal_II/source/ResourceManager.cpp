#include "ResourceManager.h"
#include <memory>
#include "Utils.h"


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
		for(ISource*  s : m_sources)
		{
			delete s;
		}

		m_sources.clear();
		m_factories.clear();
		m_memory_streams.clear();
	}

	void ResourceManager::remove_source(ISource* source) 
	{
		std::lock_guard<std::mutex> lock(m_sources_mutex);

		auto sourcePos = std::find(m_sources.begin(), m_sources.end(), source);

		ASSERT(sourcePos != m_sources.end());

		delete (*sourcePos);

		m_sources.erase(sourcePos);

	}

	SharedPtr<IStream> ResourceManager::open_resource(const path& resourcePath, bool inMemory)
	{
		if (inMemory)
		{
			std::lock_guard<std::mutex> lock(m_memory_streams_mutex);

			auto streamIt = m_memory_streams.find(resourcePath);

			if (streamIt != m_memory_streams.end())
			{
				if (SharedPtr<MemoryStream> s = streamIt->second.lock())
				{
					LOG_INFO("[ResourceManager] %s loaded from cache", resourcePath.c_str());
					return s->new_from_buffer();
				}
				else
				{
					m_memory_streams.erase(streamIt);
				}
			}

			utils::erase_if(m_memory_streams, [](const std::pair<path, WeakPtr<MemoryStream>>& stream ) { return stream.second.expired(); });
		}

		SharedPtr<IStream> stream = nullptr;

		m_sources_mutex.lock();
		for (ISource* source : m_sources) 
			if ( (stream = source->open_resource(resourcePath)) ) 
				break;
		m_sources_mutex.unlock();

		if (stream != nullptr)
		{
			LOG_INFO("[ResourceManager] %s loaded", resourcePath.c_str());
			if (inMemory)
			{
				SharedPtr<MemoryStream> memoryStream = std::make_shared<MemoryStream>(stream.get());

				std::lock_guard<std::mutex> lock(m_memory_streams_mutex);
				m_memory_streams[resourcePath] = memoryStream;

				return memoryStream;
			}
			else
			{
				return stream;
			}
		}

		LOG_ERROR("[ResourceManager] %s could not be loaded", resourcePath.c_str());

		return nullptr;
	}
}