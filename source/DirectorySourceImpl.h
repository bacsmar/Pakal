#pragma once
#include "DirectorySource.h"
#include "LogMgr.h"
#include <filesystem>
#include <fstream>
#include <memory>

namespace Pakal
{
	/// Concrete DirectorySource: opens resources as file streams rooted at a given directory.
	class DirectorySourceImpl final : public DirectorySource
	{

		std::filesystem::path m_root;
		bool m_recursive = false;

	public:
		bool initialize(const Path& path, bool recursive) override
		{
			namespace fs = std::filesystem;
			m_recursive = recursive;
			m_root = fs::path(path.c_str());

			if (!fs::exists(m_root) || !fs::is_directory(m_root))
			{
				LOG_WARNING("[DirectorySourceImpl] Path does not exist or is not a directory: %s", path.c_str());
				return false;
			}

			LOG_INFO("[DirectorySourceImpl] Mounted directory: %s", m_root.string().c_str());
			return true;
		}

		void terminate() override
		{
			m_root.clear();
		}

		SharedPtr<std::istream> open_resource(const Path& pathToResource) override
		{
			namespace fs = std::filesystem;
			fs::path candidate = m_root / fs::path(pathToResource.c_str());

			auto stream = std::make_shared<std::ifstream>(candidate.string().c_str(),
				std::ios_base::in | std::ios_base::binary);

			if (stream->is_open())
			{
				return std::static_pointer_cast<std::istream>(stream);
			}

			return nullptr;
		}
	};
}
