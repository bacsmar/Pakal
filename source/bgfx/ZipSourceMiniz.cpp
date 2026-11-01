///////////////////////////////////////////////////////////////////////////////////////////////////////
// File: ZipSourceMiniz.cpp
// Purpose: ResourceManager's zip source with miniz - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////
#include "ZipSourceMiniz.h"
#include "LogMgr.h"

#include <miniz.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_map>

namespace Pakal
{
	struct ZipSourceMiniz::Archive
	{
		mz_zip_archive zip{};
		bool open = false;
		// its files by name (lower case, '/'), made when it is mounted: a lookup instead of miniz's search
		std::unordered_map<std::string, mz_uint> index;
	};

	namespace
	{
		std::string zip_key(std::string name)
		{
			std::replace(name.begin(), name.end(), '\\', '/');
			std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return name;
		}
	}

	ZipSourceMiniz::ZipSourceMiniz() : m_archive(std::make_unique<Archive>()) {}

	ZipSourceMiniz::~ZipSourceMiniz()
	{
		terminate();
	}

	bool ZipSourceMiniz::initialize(const Path& pathToZip, bool, const std::string&)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_archive->open) mz_zip_reader_end(&m_archive->zip);
		m_archive->zip = mz_zip_archive{};
		m_archive->open = mz_zip_reader_init_file(&m_archive->zip, pathToZip.c_str(), 0) != 0;
		if (!m_archive->open)
		{
			LOG_WARNING("[ZipSource] %s is not a zip that can be read", pathToZip.c_str());
			return false;
		}
		m_archive->index.clear();
		const mz_uint count = mz_zip_reader_get_num_files(&m_archive->zip);
		char name[1024];
		for (mz_uint i = 0; i < count; ++i)
		{
			if (mz_zip_reader_is_file_a_directory(&m_archive->zip, i)) continue;
			if (mz_zip_reader_get_filename(&m_archive->zip, i, name, sizeof(name)) > 0) m_archive->index[zip_key(name)] = i;
		}
		LOG_INFO("[ZipSource] mounted %s (%u files)", pathToZip.c_str(), count);
		return true;
	}

	void ZipSourceMiniz::terminate()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_archive && m_archive->open)
		{
			mz_zip_reader_end(&m_archive->zip);
			m_archive->open = false;
		}
	}

	bool ZipSourceMiniz::has_resource(const Path& pathToResource)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_archive->open && m_archive->index.count(zip_key(pathToResource.c_str())) != 0;
	}

	SharedPtr<std::istream> ZipSourceMiniz::open_resource(const Path& pathToResource)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_archive->open) return nullptr;
		// names in a zip use '/', and the case is ignored (Path lower-cases them on Windows)
		auto found = m_archive->index.find(zip_key(pathToResource.c_str()));
		if (found == m_archive->index.end()) return nullptr;
		size_t size = 0;
		void* data = mz_zip_reader_extract_to_heap(&m_archive->zip, found->second, &size, 0);
		if (!data) return nullptr;
		auto stream = std::make_shared<std::istringstream>(std::string(static_cast<const char*>(data), size), std::ios::binary);
		mz_free(data);
		return stream;
	}
}
