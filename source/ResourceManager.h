#pragma once
#include "TaskFwd.h"

#include "LogMgr.h"
#include "Utils.h"
#include "path.h"
#include "IManager.h"
#include "ISource.h"
#include "MemoryStream.h"

#include "DirectorySource.h"
#include "ZipSource.h"

#include <map>
#include <vector>


namespace Pakal
{

	class _PAKALExport ResourceManager final : public  IManager
	{
		ResourceManager() {}

		template <class ResourceManager> friend class SingletonHolder;

		std::vector<SharedPtr<ISource>> m_sources;
		std::map<path,WeakPtr<memory_istream>> m_memory_streams;
		std::map<std::string, std::function<ISource*()>> m_factories;

		std::mutex m_memory_streams_mutex,m_sources_mutex;


	public:
		static ResourceManager& instance();

		void initialize() override;

		void terminate() override;

		template <class TSource> 
		void register_source(const std::function<TSource*()>& factory)
		{
			static_assert(std::is_base_of<ISource, TSource>::value, "type parameter of this class must derive from ISource");

			const char* name = TypeInfo::get<TSource>().getName();

			if (!m_factories.insert(std::make_pair(std::string(name), [factory]() { return static_cast<ISource*>(factory()); })).second)
			{
				LOG_ERROR("ResourceManager, %s source already registered", name);
				ASSERT(false);
			}
		}

		template<class TSource, class... arguments>
		SharedPtr<TSource> create_source(arguments&&... args)
		{
			static_assert(std::is_base_of<ISource, TSource>::value, "type parameter of this class must derive from ISource");

			std::function<ISource*()> sourceFactory;
			const char* sourceName = TypeInfo::get<TSource>().getName();

			if (!map_utils::try_get(m_factories, sourceName, sourceFactory))
			{
				LOG_ERROR("ResourceManager, %s source was not found, call register_source first", sourceName);
				ASSERT(false);
			}

			auto source = SharedPtr<TSource>( static_cast<TSource*>(sourceFactory()));

			if (source->initialize(std::forward<arguments>(args)...))
			{
				
				LOG_DEBUG("[ResourceManager] source %s created with arguments %s", sourceName, trait_utils::stringer(args...).c_str());
				return source;
			}
			else
			{
				LOG_WARNING("ResourceManager, could not add %s with arguments %s", sourceName, trait_utils::stringer(args...).c_str());
				return nullptr;
			}

		}

		template<class TSource,class... arguments> 
		SharedPtr<TSource> add_source(arguments&&... args)
		{
			SharedPtr<TSource> source = create_source<TSource>(std::forward<arguments>(args)...);

			if (source)
			{
				mutex_guard guard(m_sources_mutex);
				m_sources.push_back(std::static_pointer_cast<ISource>(source));
			}
			
			return source;
		}

		void remove_source(ISource* source);

		template<class TStream>
		SharedPtr<TStream> open_read_resource(const path& resourcePath, bool inMemory)
		{
			SharedPtr<std::istream> stream = open_read_resource(resourcePath, inMemory);

			return stream != nullptr ? std::make_shared<TStream>(stream) : nullptr;
		}

		SharedPtr<std::istream> open_read_resource(const path& resourcePath, bool inMemory);

		SharedPtr<std::ostream> open_write_resource(const path& resourcePath);	

	};

	#define ResourceMgr ResourceManager::instance()
}