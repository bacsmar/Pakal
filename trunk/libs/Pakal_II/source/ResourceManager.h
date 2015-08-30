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
		public IManager
	{		
		friend class Engine;
	public:
		
		typedef std::function<IStreamPtr(const std::string& path)> OpenReaderFunction;
		typedef std::function<bool(const std::string& path)> StringPredicate;		

	protected:
		struct StreamReaderFactory
		{
			OpenReaderFunction	open_reader;
			StringPredicate		add_file_archive;
			StringPredicate		add_data_dir;
		};

		std::vector<StreamReaderFactory> m_stream_reader_factories;	

		void initialize() override;
		void terminate() override;

	public:		
	
		ResourceManager(void);
		virtual ~ResourceManager(void);		
		
		bool add_file_archive( const std::string &path);
		bool set_data_dir( const std::string &path);
		IStreamPtr open_resource(const std::string& resourceName);

		void register_reader( const OpenReaderFunction & open_reader_function);
		void register_reader( const StreamReaderFactory& factory);
	};
}