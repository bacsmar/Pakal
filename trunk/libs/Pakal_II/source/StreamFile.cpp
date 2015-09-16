///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 PakalEngine
// File: ResourceManager.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 27 Agosto 2015
///////////////////////////////////////////////////////////////////////////////////////////////////////
#include "StreamFile.h"

namespace Pakal
{
	StreamFile::~StreamFile()
	{
		close();
	}

	StreamFile::StreamFile() : m_file(nullptr)
	{
	}

	bool StreamFile::open(const std::string& fname)
	{
		m_file_name = fname;		
		m_file = fopen(fname.c_str() , "rb");		
		return m_file;
	}

	void StreamFile::close()
	{
		if( m_file)
		fclose(m_file);
	}

	std::streamoff StreamFile::skip(size_t value)
	{
		return fseek(m_file, value, SEEK_CUR);		
	}

	std::streamoff StreamFile::tell()
	{
		return ftell(m_file);
	}

	bool StreamFile::seek(size_t offset, bool relativeMovement)
	{
		return fseek(m_file, offset, relativeMovement ? SEEK_CUR : SEEK_SET) == 0;
	}

	std::streamoff StreamFile::size()
	{
		auto currPosition = tell();
		fseek(m_file, 0, SEEK_END);
		auto	m_file_size = tell();
		fseek(m_file, currPosition, SEEK_SET);	
		return m_file_size;		
	}

	std::streamoff StreamFile::read(void* buf, std::size_t count)
	{
		return fread(buf, 1, count, m_file);
	}

	const char* StreamFile::get_file_name()
	{
		return m_file_name.c_str();
	}
}