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

Pakal::IStreamPtr Pakal::ResourceManager::openResource(const std::string& resourceName)
{
	for( auto& open_reader_function : m_stream_reader_factories)
	{
		auto reader = open_reader_function(resourceName);
		if( reader.get() )
		{
			return reader;
		}
	}
	return nullptr;
}

void Pakal::ResourceManager::register_reader(const OpenReaderFunction & open_reader_function)
{	
	m_stream_reader_factories.push_back( open_reader_function );
}