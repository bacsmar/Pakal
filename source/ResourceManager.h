///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 PakalEngine
// File: ResourceManager.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 27 Agosto 2015
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "IManager.h"
#include <string>
#include "IStream.h"
#include <vector>

namespace Pakal
{
	
	// TODO: acceso concurrente
	class _PAKALExport ResourceManager final
	{
		template <class ResourceManager> friend class SingletonHolder;
	public:
		class IFileArchive
		{
		protected:
			~IFileArchive(){}
		public:
			virtual IStreamPtr		open_reader(const std::string&  path) = 0;
			virtual IFileArchive*	add_file_archive(IStreamPtr file){ return nullptr; };
			virtual IFileArchive*	add_data_dir(const std::string&  path){ return nullptr; };
		};

	protected:		

		std::vector<IFileArchive*> m_stream_sources;	

		ResourceManager(void);
		~ResourceManager(void);
	public:					
		static ResourceManager& instance();
		bool add_file_archive( const std::string &path);
		bool add_data_dir( const std::string &path);
		IStreamPtr open_resource(const std::string& resourceName);
		
		void register_reader( IFileArchive* factory);
		void remove_reader( IFileArchive* factory);
	};
}