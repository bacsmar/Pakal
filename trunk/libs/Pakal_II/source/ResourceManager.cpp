#include "ResourceManager.h"
#include <memory>


namespace Pakal
{
	std::string ResourceManager::normalize_path(std::string str) 
	{
		std::replace(str.begin(), str.end(), '\\', '/');
		std::transform(str.begin(), str.end(), str.begin(), ::tolower);
		return str;
	}

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

	SharedPtr<IStream> ResourceManager::open_resource(const std::string& resourcePath, bool inMemory)
	{
		if (inMemory)
		{
			std::lock_guard<std::mutex> lock(m_memory_streams_mutex);

			std::string normalizedPath = normalize_path(resourcePath);

			auto streamIt = m_memory_streams.find(normalizedPath);

			if (streamIt != m_memory_streams.end())
			{
				if (SharedPtr<MemoryStream> s = streamIt->second.lock())
				{
					LOG_INFO("[ResourceManager] %s loaded from cache", resourcePath.c_str());
					return s->new_from_buffer();
				}
				else
					m_memory_streams.erase(streamIt);
			}
			//erase_if(m_memory_streams, [](const auto& stream) { return stream.second.expired(); });			
			using DataType = std::pair<std::string, WeakPtr<MemoryStream>> ;

			erase_if(m_memory_streams, [](const DataType& stream ) { return stream.second.expired(); });
		}

		SharedPtr<IStream> stream = nullptr;
		{
			std::lock_guard<std::mutex> lock(m_sources_mutex);
			for (ISource* source : m_sources)
			{
				stream = source->open_resource(resourcePath);
				if (stream != nullptr)
				{
					break;
				}
			}
		}

		if (stream != nullptr)
		{
			LOG_INFO("[ResourceManager] %s loaded", resourcePath.c_str());
			if (inMemory)
			{
				SharedPtr<MemoryStream> memoryStream = std::make_shared<MemoryStream>(stream.get());

				std::lock_guard<std::mutex> lock(m_memory_streams_mutex);
				m_memory_streams[normalize_path(resourcePath)] = memoryStream;

				return memoryStream;
			}
			else
				return stream;
		}

		LOG_ERROR("[ResourceManager] %s could not be loaded", resourcePath.c_str());

		return nullptr;
	}
}