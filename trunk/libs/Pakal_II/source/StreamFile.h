///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 PakalEngine
// File: ResourceManager.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 27 Agosto 2015
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Config.h"
#include "IStream.h"
#include <cstdio>

namespace Pakal
{
	class _PAKALExport StreamFile : public IStream
	{		
	
		FILE *		m_file;
		std::string	m_file_name;		
	public:
		virtual ~StreamFile(void);
		StreamFile();

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
}