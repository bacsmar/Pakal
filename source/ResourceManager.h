#pragma once
#include "TaskFwd.h"

#include "LogMgr.h"
#include "Utils.h"
#include "PakalPath.h"
#include "IManager.h"
#include "ISource.h"
#include "MemoryStream.h"

#include "DirectorySource.h"
#include "ZipSource.h"

#include <cstdint>
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <string>


namespace Pakal
{

	class _PAKALExport ResourceManager final : public  IManager
	{
		ResourceManager() {}

		template <class ResourceManager> friend class SingletonHolder;

		std::vector<SharedPtr<ISource>> m_sources;
		std::map<Path,WeakPtr<memory_istream>> m_memory_streams;
		std::map<std::string, std::function<ISource*()>> m_factories;
		std::string m_root_path;

		std::mutex m_memory_streams_mutex,m_sources_mutex;


	public:
		static ResourceManager& instance();

		void initialize() override;

		void terminate() override;

		void set_root_path(const std::string& root_path);
		const std::string& get_root_path() const { return m_root_path; }
		std::string resolve_file_path(const std::string& resource_path) const;

		template <class TSource> 
		void register_source(const std::function<TSource*()>& factory)
		{
			static_assert(std::is_base_of<ISource, TSource>::value, "type parameter of this class must derive from ISource");

			auto name = TypeInfo::get<TSource>().get_name();

			if (!m_factories.insert(std::make_pair(std::string(name), [factory]() { return static_cast<ISource*>(factory()); })).second)
			{
				LOG_ERROR("ResourceManager, %s source already registered", name.c_str());
				ASSERT(false);
			}
		}

		// Whether a kind of source can be made (its factory is registered)
		template <class TSource>
		bool has_source_factory() const
		{
			return m_factories.count(std::string(TypeInfo::get<TSource>().get_name())) != 0;
		}

		template<class TSource, class... arguments>
		SharedPtr<TSource> create_source(arguments&&... args)
		{
			static_assert(std::is_base_of<ISource, TSource>::value, "type parameter of this class must derive from ISource");

			std::function<ISource*()> sourceFactory;
			auto sourceName = TypeInfo::get<TSource>().get_name();

			if (!map_utils::try_get(m_factories, sourceName, sourceFactory))
			{
				LOG_ERROR("ResourceManager, %s source was not found, call register_source first", sourceName.c_str());
				ASSERT(false);
			}

			auto source = SharedPtr<TSource>( static_cast<TSource*>(sourceFactory()));

			if (source->initialize(std::forward<arguments>(args)...))
			{
				
				LOG_DEBUG("[ResourceManager] source %s created with arguments %s", sourceName.c_str(), file_utils::stringer(args...).c_str());
				return source;
			}
			else
			{
				LOG_WARNING("ResourceManager, could not add %s with arguments %s", sourceName.c_str(), file_utils::stringer(args...).c_str());
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
			forget_known();
			
			return source;
		}

		void remove_source(ISource* source);

		template<class TStream>
		SharedPtr<TStream> open_read_resource(const Path& resourcePath, bool inMemory = false)
		{
			SharedPtr<std::istream> stream = open_read_resource(resourcePath, inMemory);

			return stream != nullptr ? std::make_shared<TStream>(stream) : nullptr;
		}

		SharedPtr<std::istream> open_read_resource(const Path& resourcePath, bool inMemory = false);

		SharedPtr<std::ostream> open_write_resource(const Path& resourcePath);

		// A whole file as bytes, from wherever it is: the path as given, under the root folder, or in a mounted source (a
		// folder, a zip, the APK's assets on Android), in that order. The bytes are shared while anyone holds them: a model,
		// a texture or a document read again meanwhile costs nothing. Null when no source has it. Any thread.
		SharedPtr<const std::vector<uint8_t>> read_bytes(const std::string& path);
		// The same as text (a JSON document, a script); empty when it is not found ('found' says which)
		std::string read_text(const std::string& path, bool* found = nullptr);
		// Whether a file is there, in any of those places
		bool exists(const std::string& path);
		// A resource's name as the game knows it, wherever it was found: normalized ('/', no "." or ".."), and without the
		// root folder when it is under it ("<root>/Assets/x/../y.png" -> "Assets/y.png"). The same on every platform, so
		// two names can be compared or made relative to each other.
		std::string logical_path(const std::string& path) const;

	private:
		SharedPtr<std::istream> open_from_any(const std::string& path);
		std::map<std::string, WeakPtr<const std::vector<uint8_t>>> m_bytes;
		std::mutex m_bytes_mutex;
		// exists()'s answers, by path: the files do not change while the game runs; mounting or removing a source, a new
		// root or a file written forget them
		std::unordered_map<std::string, bool> m_known;
		std::mutex m_known_mutex;
		void forget_known();
		bool find_anywhere(const std::string& path);
	public:

	};

	#define ResourceMgr Pakal::ResourceManager::instance()
}