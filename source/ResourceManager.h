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
#include <functional>


namespace Pakal
{
	
	// TODO: acceso concurrente
	class _PAKALExport ResourceManager :
		public Pakal::IManager
	{		
		friend class Engine;
	public:
		
		typedef std::function<IStreamPtr(const std::string&)> OpenReaderFunction;		
		typedef std::function<bool(const std::string& resource_name)> AddZipFileFunction;
		//typedef std::function<void(const std::string& resource_name, std::string& out_path_string)> FindResourcePathFunction;

	protected:
		struct StreamReaderFactory
		{
			OpenReaderFunction	open_reader;
			AddZipFileFunction	add_zip_file;
			//FindResourcePathFunction	find_resource_path;
		};

		std::vector<StreamReaderFactory> m_stream_reader_factories;	

		void initialize() override;
		void terminate() override;

	public:		
	
		ResourceManager(void);
		virtual ~ResourceManager(void);		

		//void find_resource_full_path( const std::string& resourceName, std::string& outfileFullPath);	// TODO		
		bool add_file_archive( const std::string &path);
		IStreamPtr open_resource(const std::string& resourceName);

		void register_reader( const OpenReaderFunction & open_reader_function);
	};
}