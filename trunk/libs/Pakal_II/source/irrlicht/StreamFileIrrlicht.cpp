///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 PakalEngine
// File: ResourceManager.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 27 Agosto 2015
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "StreamFileIrrlicht.h"

namespace Pakal
{	
	StreamFileIrrlicht::~StreamFileIrrlicht()
	{
		close();
	}

	StreamFileIrrlicht::StreamFileIrrlicht(irr::io::IReadFile* f) : m_file(f)
	{
	}	

	bool StreamFileIrrlicht::open(const std::string& fname)
	{
		return false;
	}

	void StreamFileIrrlicht::close()
	{
		m_file->drop();
		m_file = nullptr;
	}

	std::streamoff StreamFileIrrlicht::skip(size_t value)
	{		
		m_file->seek(value, true);
		return 0;
	}

	std::streamoff StreamFileIrrlicht::tell()
	{
		return m_file->getPos();
	}

	void StreamFileIrrlicht::seek(size_t offset)
	{
		m_file->seek(offset);
	}

	std::streamoff StreamFileIrrlicht::size()
	{
		return m_file->getSize();
	}

	std::streamoff StreamFileIrrlicht::read(void* buf, std::size_t count)
	{
		return m_file->read(static_cast<char*>(buf), count);
	}
	
}