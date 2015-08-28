#include "ResourceManager.h"
#include "StreamFile.h"


void Pakal::ResourceManager::initialize()
{
	// register default sources...	
	register_reader( &StreamFile::open_reader);
}

void Pakal::ResourceManager::terminate()
{
	m_stream_reader_factories.clear();
}

Pakal::ResourceManager::ResourceManager(void)
{
}


Pakal::ResourceManager::~ResourceManager(void)
{
}

bool Pakal::ResourceManager::add_file_archive(const std::string& path)
{
	for( auto& factory : m_stream_reader_factories)
	{		
		if( factory.add_zip_file && factory.add_zip_file(path) )
		{			
			return true;
		}
	}
	return false;
}

Pakal::IStreamPtr Pakal::ResourceManager::open_resource(const std::string& resourceName)
{
	for( auto& factory : m_stream_reader_factories)
	{
		auto reader = factory.open_reader(resourceName);
		if( reader.get() )
		{
			return reader;
		}
	}
	return nullptr;
}

void Pakal::ResourceManager::register_reader(const OpenReaderFunction & open_reader_function)
{	
	StreamReaderFactory factory;
	factory.open_reader = open_reader_function;	
	m_stream_reader_factories.push_back( factory );
}