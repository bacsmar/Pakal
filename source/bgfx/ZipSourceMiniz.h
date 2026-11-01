///////////////////////////////////////////////////////////////////////////////////////////////////////
// File: ZipSourceMiniz.h
// Purpose: ResourceManager's zip source, with miniz (which bimg already brings): the files of a .zip
//          read as if they were on the disk, by their path inside it (a game's assets in one archive, a
//          mod, a pack downloaded later). Mounted with ResourceMgr.add_source<ZipSource>(path, true).
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "ZipSource.h"

#include <memory>
#include <mutex>
#include <string>

namespace Pakal
{
	class ZipSourceMiniz final : public ZipSource
	{
	public:
		ZipSourceMiniz();
		~ZipSourceMiniz() override;

		// 'recursiveDirs' and 'password' are not used: every file of the zip is reached by its whole path, and miniz reads no
		// encrypted entries
		bool initialize(const Path& pathToZip, bool recursiveDirs, const std::string& password = "") override;
		void terminate() override;
		SharedPtr<std::istream> open_resource(const Path& pathToResource) override;
		bool has_resource(const Path& pathToResource) override;

	private:
		struct Archive;
		std::unique_ptr<Archive> m_archive;
		std::mutex m_mutex;   // miniz's reader is not safe from two threads at once
	};
}
