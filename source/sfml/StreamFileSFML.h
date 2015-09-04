///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 PakalEngine
// File: ResourceManager.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 27 Agosto 2015
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Config.h"
#include "IStream.h"
#include <SFML/System.hpp>

namespace Pakal
{
	// used to read from  irrlicht to pakal
	class _PAKALExport StreamReaderSFML : public IStream
	{
		sf::InputStream* m_stream;
		std::string m_file_name;
	public:
		virtual ~StreamReaderSFML(void);
		StreamReaderSFML(sf::InputStream* f);

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

	// Used to read from pakal to Irrlicht
	class SFMLReadPakalFile : public sf::InputStream
	{
	
		IStreamPtr			m_pakal_stream;
	public:		

		explicit SFMLReadPakalFile(IStreamPtr p);

		~SFMLReadPakalFile() override{}	
		
		sf::Int64 read(void* data, sf::Int64 size) override;
		sf::Int64 seek(sf::Int64 position) override;
		sf::Int64 tell() override;
		sf::Int64 getSize() override;	
	};
}