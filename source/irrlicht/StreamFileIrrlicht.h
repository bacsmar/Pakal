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
	class _PAKALExport StreamFileIrrlicht : public IStream
	{
		irr::io::IReadFile *m_file;
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
		virtual void seek(size_t offset) override;

		//
		virtual std::streamoff size() override;

		//
		virtual std::streamoff read(void* buf, std::size_t count) override;				
	};	
}