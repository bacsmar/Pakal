#pragma once
#include "Config.h"

#include "IFileSystem.h"

#include "DirectorySource.h"
#include "IrrlitchStream.h"

namespace Pakal
{
	class _PAKALExport DirectorySourceIrrlitch : public DirectorySource
	{
		DECLARE_RTTI_WITH_BASE(DirectorySourceIrrlitch, DirectorySource);

		irr::io::IFileSystem* m_file_system;
		irr::io::IFileArchive* m_archive;
	public:


		explicit DirectorySourceIrrlitch(irr::io::IFileSystem* fileSystem) : m_file_system(fileSystem), m_archive(nullptr) {}

		bool initialize(const Path& path, bool recursive) override
		{
			return m_file_system->addFileArchive(path.c_str(), true, !recursive, irr::io::EFAT_FOLDER, "", &m_archive);
		}

		void terminate() override
		{
			m_file_system->removeFileArchive(m_archive);
		}

		SharedPtr<std::istream> open_resource(const Path& pathToResource) override
		{
			irr::io::IReadFile * nativeFile = m_archive->createAndOpenFile(pathToResource.c_str());

			return nativeFile ? std::make_shared<IrrlitchStream>(nativeFile) : nullptr;
		}

	};

}