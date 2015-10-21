#pragma once
#include "Config.h"

#include "IFileSystem.h"

#include "ZipSource.h"
#include "IrrlitchStream.h"

namespace Pakal
{
	class _PAKALExport ZipSourceIrrlitch : public ZipSource
	{
		DECLARE_RTTI_WITH_BASE(ZipSourceIrrlitch, ZipSource);
		irr::io::IFileSystem* m_file_system;
		irr::io::IFileArchive* m_archive;
	public:

		explicit ZipSourceIrrlitch(irr::io::IFileSystem* fileSystem) : m_file_system(fileSystem), m_archive(nullptr) {}

		bool initialize(const path& path, bool recursive, const std::string& password = "") override
		{
			return m_file_system->addFileArchive(path.c_str(), true, !recursive, irr::io::EFAT_ZIP, password.c_str(), &m_archive);
		}

		void terminate() override
		{
			m_file_system->removeFileArchive(m_archive);
		}

		SharedPtr<std::istream> open_resource(const path& pathToResource) override
		{
			irr::io::IReadFile* nativeFile = m_archive->createAndOpenFile(pathToResource.c_str());

			return nativeFile ? std::make_shared<IrrlitchStream>(nativeFile) : nullptr;
		}

	};


}