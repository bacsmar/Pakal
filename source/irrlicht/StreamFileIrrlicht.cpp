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

	StreamFileIrrlicht::StreamFileIrrlicht(irr::io::IReadFile* f) : m_irr_file(f)
	{
	}	

	bool StreamFileIrrlicht::open(const std::string& fname)
	{
		return false;
	}

	void StreamFileIrrlicht::close()
	{
		m_irr_file->drop();
		m_irr_file = nullptr;
	}

	std::streamoff StreamFileIrrlicht::skip(size_t value)
	{		
		m_irr_file->seek(value, true);
		return 0;
	}

	std::streamoff StreamFileIrrlicht::tell()
	{
		return m_irr_file->getPos();
	}

	bool StreamFileIrrlicht::seek(size_t offset, bool relativeMovement)
	{
		return m_irr_file->seek(offset, relativeMovement);
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	std::streamoff StreamFileIrrlicht::size()
	{
		return m_irr_file->getSize();
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	std::streamoff StreamFileIrrlicht::read(void* buf, std::size_t count)
	{
		return m_irr_file->read(static_cast<char*>(buf), count);
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	const char* StreamFileIrrlicht::get_file_name()
	{
		return m_irr_file->getFileName().c_str();
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	irr::s32 IrrReadPakalFile::read(void* buffer, irr::u32 sizeToRead)
	{ return m_pakal_stream->read(buffer, sizeToRead); }
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	bool IrrReadPakalFile::seek(long finalPos, bool relativeMovement)
	{ return m_pakal_stream->seek(finalPos, relativeMovement); }
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	long IrrReadPakalFile::getSize() const
	{ return m_pakal_stream->size(); }
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	long IrrReadPakalFile::getPos() const
	{ return m_pakal_stream->tell(); }
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	const irr::io::path& IrrReadPakalFile::getFileName() const
	{ return m_file_name; }
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	IrrReadPakalFile::IrrReadPakalFile(IStreamPtr p): m_pakal_stream(p), m_file_name(p->get_file_name())
	{}
}