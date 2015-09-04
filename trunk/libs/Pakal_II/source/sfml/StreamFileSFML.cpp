///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 PakalEngine
// File: ResourceManager.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 27 Agosto 2015
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "StreamFileSFML.h"

namespace Pakal
{	
	StreamReaderSFML::~StreamReaderSFML()
	{
		close();
	}

	StreamReaderSFML::StreamReaderSFML(sf::InputStream* f) : m_stream(f)
	{
	}	

	bool StreamReaderSFML::open(const std::string& fname)
	{
		return false;
	}

	void StreamReaderSFML::close()
	{
		SAFE_DEL(m_stream);
	}

	std::streamoff StreamReaderSFML::skip(size_t value)
	{		
		m_stream->seek(value);
		return 0;
	}

	std::streamoff StreamReaderSFML::tell()
	{
		return m_stream->tell();
	}

	bool StreamReaderSFML::seek(size_t offset, bool relativeMovement)
	{
		return m_stream->seek(offset);
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	std::streamoff StreamReaderSFML::size()
	{
		return m_stream->getSize();
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	std::streamoff StreamReaderSFML::read(void* buf, std::size_t count)
	{
		return m_stream->read(buf, count);
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	const char* StreamReaderSFML::get_file_name()
	{
		return m_file_name.c_str();
	}
	///////////////////////////////////////////////////////////////////////////////////////////////////////
	SFMLReadPakalFile::SFMLReadPakalFile(IStreamPtr p) : m_pakal_stream(p)
	{
	}
	
	sf::Int64 SFMLReadPakalFile::read(void* data, sf::Int64 size)
	{
		return m_pakal_stream->read(data, size);
	}

	sf::Int64 SFMLReadPakalFile::seek(sf::Int64 position)
	{
		return !m_pakal_stream->seek(position); // in sfml: <> 0 means error
	}

	sf::Int64 SFMLReadPakalFile::tell()
	{
		return m_pakal_stream->tell();
	}

	sf::Int64 SFMLReadPakalFile::getSize()
	{
		return m_pakal_stream->size();
	}
}