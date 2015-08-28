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
	

	class _PAKALExport ResourceManager :
		public Pakal::IManager
	{		
	public:
		
		typedef std::function<IStreamPtr(const std::string&)> OpenReaderFunction;
		typedef std::function<void(const std::string& resource_name, std::string& out_path_string)> FindResourcePathFunction;

	protected:
		struct stream_reader_factory
		{
			OpenReaderFunction			m_open_reader_function;
			//FindResourcePathFunction	m_find_resource_path_function;	// TODO
		};

		std::vector<OpenReaderFunction> m_stream_reader_factories;	// TODO... guardar la estructura?

	public:
		void initialize() override;
		void terminate() override;
	
		ResourceManager(void);
		virtual ~ResourceManager(void);		

		//void findResourceFullPath( const std::string& resourceName, std::string& fileFullPath);	// TODO
		IStreamPtr openResource(const std::string& resourceName);

		void register_reader( const OpenReaderFunction & open_reader_function);
	};
}