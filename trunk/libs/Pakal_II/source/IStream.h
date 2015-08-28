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
		virtual void				seek(size_t offset) = 0;
		//
		virtual std::streamoff		size() = 0;	
		//
		virtual std::streamoff read(void* buf, std::size_t count) = 0;

		/*
		virtual std::size_t readLine(char* buf, std::size_t maxCount, const std::string& delim = "\n") = 0;
		virtual std::string getAsString(void) const = 0;
		virtual void close() = 0;

		virtual const void * getbuffer() const {return nullptr;};
		virtual std::size_t write(const void* buf, std::size_t count){ return 0; }
		*/
	};

	typedef std::shared_ptr<IStream> IStreamPtr;
}