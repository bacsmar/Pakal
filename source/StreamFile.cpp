///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 PakalEngine
// File: ResourceManager.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 27 Agosto 2015
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "StreamFile.h"

namespace Pakal
{
	// static
	IStreamPtr StreamFile::open_reader(const std::string& fname)
	{
		auto file = new StreamFile();
		IStreamPtr p(file);

		return file->open(fname) ? p : nullptr;		
	}

	StreamFile::~StreamFile()
	{
		close();
	}

	bool StreamFile::open(const std::string& fname)
	{
		m_file.open(fname.c_str());
		return m_file.is_open();
	}

	void StreamFile::close()
	{
		m_file.close();			
	}

	std::streamoff StreamFile::skip(size_t value)
	{
		m_file.seekg(value,std::ios::cur);
		return 0;
	}

	std::streamoff StreamFile::tell()
	{
		return m_file.tellg();
	}

	void StreamFile::seek(size_t offset)
	{
		m_file.seekg(offset);
	}

	std::streamoff StreamFile::size()
	{
		auto cur = m_file.tellg();
		m_file.seekg (0, std::ios::end);
		auto length = m_file.tellg();
		m_file.seekg (cur, std::ios::beg);
		return length;
	}

	std::streamoff StreamFile::read(void* buf, std::size_t count)
	{
		m_file.read(static_cast<char*>(buf), count);
		return 0;
	}
	
}