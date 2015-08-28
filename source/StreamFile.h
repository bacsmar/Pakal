///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 PakalEngine
// File: ResourceManager.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 27 Agosto 2015
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Config.h"
#include "IStream.h"
#include <fstream>

namespace Pakal
{
	class _PAKALExport StreamFile : public Pakal::IStream
	{
		std::ifstream m_file;
	public:
		virtual ~StreamFile(void);

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
		// used by 
		static IStreamPtr open_reader(const std::string& fname);
	};	
}