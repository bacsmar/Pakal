///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 PakalEngine
// File: ResourceManager.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 27 Agosto 2015
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Config.h"
#include "IStream.h"
#include <irrlicht.h>

namespace Pakal
{
	// used to read from  irrlicht to pakal
	class _PAKALExport StreamFileIrrlicht : public IStream
	{
		irr::io::IReadFile *m_irr_file;
	public:
		virtual ~StreamFileIrrlicht(void);
		StreamFileIrrlicht(irr::io::IReadFile *f);

		bool open(const std::string& fname);
		void close();

		// change input stream position by _Off, according to _Way
		virtual std::streamoff skip(size_t value) override;

		// return input stream position
		virtual std::streamoff tell() override;

		// set input stream position to _Pos
		virtual bool seek(size_t offset, bool relativeMovement) override;

		//
		virtual std::streamoff size() override;

		//
		virtual std::streamoff read(void* buf, std::size_t count) override;				
		const char* get_file_name() override;	
	};	

	// Used to read from pakal to Irrlicht
	class IrrReadPakalFile : public irr::io::IReadFile
	{
		IStreamPtr			m_pakal_stream;
		const irr::io::path m_file_name;
	public:
		irr::s32 read(void* buffer, irr::u32 sizeToRead) override;

		bool seek(long finalPos, bool relativeMovement) override;

		long getSize() const override;

		long getPos() const override;

		const irr::io::path& getFileName() const override;

		explicit IrrReadPakalFile(IStreamPtr p);

		~IrrReadPakalFile() override{}	
	};
}