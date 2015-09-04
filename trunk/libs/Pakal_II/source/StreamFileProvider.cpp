///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 PakalEngine
// File: ResourceManager.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 27 Agosto 2015
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "StreamFileProvider.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "StreamFile.h"

namespace Pakal
{
	IStreamPtr StreamFileProvider::open_reader(const std::string& path)
	{
		auto file = new StreamFile();
		IStreamPtr p(file);

		return file->open( m_directory_prefix + path) ? p : nullptr;
	}

	ResourceManager::IFileArchive* StreamFileProvider::add_data_dir(const std::string& path)
	{
		/*
		struct stat info;		
		stat( path.c_str(), &info );
		if(  info.st_mode != S_IFDIR  )
		{
			return nullptr;
		}
		*/
		return new StreamFileProvider(path);
	}	
}