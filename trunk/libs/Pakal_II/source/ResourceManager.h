#pragma once
#include "LogMgr.h"
#include "SingletonHolder.h"
#include "IManager.h"
#include "TaskFwd.h"
#include "ISource.h"
#include "IStream.h"
#include "MemoryStream.h"

#include <map>
#include <vector>
#include <algorithm>

#include "DirectorySource.h"
#include "ZipSource.h"
#include "path.h"

namespace Pakal
{

	class _PAKALExport ResourceManager final : public  IManager
	{
		ResourceManager() {}

		template <class ResourceManager> friend class SingletonHolder;

		std::map<std::string, std::function<ISource*()>> m_factories;
		std::vector<ISource*> m_sources;
		std::map<path,WeakPtr<MemoryStream>> m_memory_streams;

		std::mutex m_memory_streams_mutex,m_sources_mutex;


		template<class TSource, class... arguments> 
		TSource* internal_create(arguments&&... args)
		{
			static_assert(std::is_base_of<ISource, TSource>::value, "type parameter of this class must derive from ISource");

			std::string name = TypeInfo::get<TSource>().getName();

			auto factory = m_factories.find(name);

			if (factory == m_factories.end())
			{
				LOG_ERROR("ResourceManager, %s source was not found, call register_source first", name.c_str());
				ASSERT(false);
			}

			ISource* source = (*factory).second();

			TSource* tSource = static_cast<TSource*>(source);

			if (tSource->initialize(std::forward<arguments>(args)...))
			{
				LOG_DEBUG("[ResourceManager] source %s created",name.c_str());
				return tSource;
			}
			else
			{
				LOG_WARNING("ResourceManager, could not add %s with the provided parameters", name.c_str());
				delete tSource;
				return nullptr;
			}

		}

	public:
		static ResourceManager& instance();

		void initialize() override;

		void terminate() override;

		template <class TSource> 
		void register_source(const std::function<TSource*()>& factory)
		{
			static_assert(std::is_base_of<ISource, TSource>::value, "type parameter of this class must derive from ISource");

			std::string name = TypeInfo::get<TSource>().getName();

			if (m_factories.find(name) != m_factories.end())
			{
				LOG_ERROR("ResourceManager, %s source already registered",name.c_str());
				ASSERT(false);
			}

			m_factories[name] =  [factory]() { return static_cast<ISource*>(factory()); };
		}

		template<class TSource,class... arguments> 
		TSource* add_source(arguments&&... args)
		{
			TSource* source = internal_create<TSource>(std::forward<arguments>(args)...);

			if (source)
			{
				std::lock_guard<std::mutex> lock(m_sources_mutex);
				m_sources.push_back(source);
				return source;
			}

			return nullptr;
		}

		template<class TSource, class... arguments>
		SharedPtr<TSource> create_source(arguments&&... args)
		{
			TSource* source = internal_create<TSource>(std::forward<arguments>(args)...);

			return source ? SharedPtr<TSource>(source) : nullptr;
		}

		void remove_source(ISource* source);

		SharedPtr<IStream> open_write_resource(const path& resourcePath, WriteMode mode);

		SharedPtr<IStream> open_read_resource(const path& resourcePath, bool inMemory);

		template<class TStream> 
		SharedPtr<TStream> open_read_resource(const path& resourcePath, bool inMemory)
		{
			SharedPtr<IStream> stream = open_read_resource(resourcePath, inMemory);

			return stream != nullptr ? std::make_shared<TStream>(stream) : nullptr;
		}

	};
}