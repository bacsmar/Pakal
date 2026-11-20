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

		// 'recursiveDirs' is not used: every file of the zip is reached by its whole path. 'password': the key of a sealed pack. An
		// entry that starts with "PKP1" is sealed (tools/pakal_pack.py): its bytes are deflated and then mixed with a stream made
		// from the key and the entry's name, and are read back only with the same key. Other entries read as they are. It keeps a
		// game's art from being opened by hand; it is not strong encryption (the key lives in the game)
		bool initialize(const Path& pathToZip, bool recursiveDirs, const std::string& password = "") override;
		void terminate() override;
		SharedPtr<std::istream> open_resource(const Path& pathToResource) override;
		bool has_resource(const Path& pathToResource) override;
		bool resource_size(const Path& pathToResource, uint64_t& size) override;

	private:
		struct Archive;
		std::unique_ptr<Archive> m_archive;
		std::string m_password;
		std::mutex m_mutex;   // miniz's reader is not safe from two threads at once
	};
}
