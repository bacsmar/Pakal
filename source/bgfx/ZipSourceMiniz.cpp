///////////////////////////////////////////////////////////////////////////////////////////////////////
// File: ZipSourceMiniz.cpp
// Purpose: ResourceManager's zip source with miniz - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////
#include "ZipSourceMiniz.h"
#include "LogMgr.h"
#include "ResourceManager.h"

#include <miniz.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
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
			// a path under the resource manager's root (<root>/Assets/...) is asked as the pack names it (Assets/...), as the
			// other sources take it (AndroidAssetSource, a folder)
			std::string root = ResourceMgr.get_root_path();
			if (!root.empty() && root != ".")
			{
				std::replace(root.begin(), root.end(), '\\', '/');
				std::transform(root.begin(), root.end(), root.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				if (root.back() != '/') root += '/';
				if (name.compare(0, root.size(), root) == 0) name.erase(0, root.size());
			}
			return name;
		}

		// The stream a sealed entry is mixed with: xorshift64* seeded by FNV-1a of the key, '|' and the entry's name (tools/pakal_pack.py)
		void unseal(std::string& bytes, const std::string& key, const std::string& name)
		{
			uint64_t h = 1469598103934665603ull;
			for (char c : key + "|" + name) { h ^= static_cast<unsigned char>(c); h *= 1099511628211ull; }
			uint64_t s = h ? h : 0x9E3779B97F4A7C15ull;
			for (size_t i = 0; i < bytes.size(); i += 8)
			{
				s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
				const uint64_t k = s * 2685821657736338717ull;
				for (size_t j = 0; j < 8 && i + j < bytes.size(); ++j) bytes[i + j] = static_cast<char>(bytes[i + j] ^ static_cast<char>((k >> (8 * j)) & 0xFF));
			}
		}
	}

	ZipSourceMiniz::ZipSourceMiniz() : m_archive(std::make_unique<Archive>()) {}

	ZipSourceMiniz::~ZipSourceMiniz()
	{
		terminate();
	}

	bool ZipSourceMiniz::initialize(const Path& pathToZip, bool, const std::string& password)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_password = password;
		if (m_archive->open) mz_zip_reader_end(&m_archive->zip);
		m_archive->zip = mz_zip_archive{};
		m_archive->open = mz_zip_reader_init_file(&m_archive->zip, pathToZip.c_str(), 0) != 0;
		if (!m_archive->open)
		{
			// not a file on the disk: a pack another source shows in place (one kept uncompressed in the APK), read where it lies
			const void* data = nullptr;
			size_t size = 0;
			m_archive->zip = mz_zip_archive{};
			if (ResourceMgr.map_resource(pathToZip.c_str(), data, size))
				m_archive->open = mz_zip_reader_init_mem(&m_archive->zip, data, size, 0) != 0;
		}
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

	bool ZipSourceMiniz::resource_size(const Path& pathToResource, uint64_t& size)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_archive->open) return false;
		auto found = m_archive->index.find(zip_key(pathToResource.c_str()));
		mz_zip_archive_file_stat stat;
		if (found == m_archive->index.end() || !mz_zip_reader_file_stat(&m_archive->zip, found->second, &stat)) return false;
		size = stat.m_uncomp_size;
		return true;
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
		std::string bytes(static_cast<const char*>(data), size);
		mz_free(data);
		if (bytes.size() >= 8 && bytes.compare(0, 4, "PKP1") == 0)
		{
			// sealed: "PKP1", the size it has open (4 bytes, little endian), then the deflated bytes mixed with the key's stream
			if (m_password.empty()) return nullptr;
			const uint32_t open = static_cast<uint8_t>(bytes[4]) | (static_cast<uint8_t>(bytes[5]) << 8) | (static_cast<uint8_t>(bytes[6]) << 16) | (static_cast<uint32_t>(static_cast<uint8_t>(bytes[7])) << 24);
			std::string body = bytes.substr(8);
			unseal(body, m_password, zip_key(pathToResource.c_str()));
			std::string out(open, static_cast<char>(0));
			mz_ulong outSize = open;
			if (mz_uncompress(reinterpret_cast<unsigned char*>(out.data()), &outSize, reinterpret_cast<const unsigned char*>(body.data()), static_cast<mz_ulong>(body.size())) != MZ_OK || outSize != open)
			{
				LOG_WARNING("[ZipSource] %s could not be opened (a wrong key?)", pathToResource.c_str());
				return nullptr;
			}
			bytes.swap(out);
		}
		return std::make_shared<std::istringstream>(std::move(bytes), std::ios::binary);
	}
}
