///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 PakalEngine
// File: ResourceManager.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 27 Agosto 2015
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Config.h"
#include "IStream.h"
#include "ResourceManager.h"

namespace Pakal
{
	class _PAKALExport StreamFileProvider : public ResourceManager::IFileArchive
	{
		std::string m_directory_prefix;
	public:
		IStreamPtr open_reader(const std::string& path) override;
		IFileArchive* add_data_dir(const std::string& path) override;
		//IFileArchive* add_file_archive(const std::string& path) override{};	// do nothing		
	
		virtual ~StreamFileProvider()
		{
		}

		explicit StreamFileProvider(const std::string & prefix) : m_directory_prefix(prefix){}
		StreamFileProvider() {}
	};	
}