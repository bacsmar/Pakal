///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 PakalEngine
// File: ResourceManager.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 27 Agosto 2015
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Config.h"
#include <memory>
#include <string>

namespace Pakal
{
	class _PAKALExport IStream
	{		
	public:
		virtual ~IStream() {}

		// change input stream position by _Off, according to _Way
		virtual std::streamoff		skip(size_t value) = 0;
		// return input stream position
		virtual std::streamoff		tell() = 0;
		// set input stream position to _Pos
		virtual bool				seek(size_t offset, bool relativeMovement = false) = 0;
		//
		virtual std::streamoff		size() = 0;	
		//
		virtual std::streamoff read(void* buf, std::size_t count) = 0;

		virtual const char*			get_file_name() = 0;
	};

	typedef std::shared_ptr<IStream> IStreamPtr;
}