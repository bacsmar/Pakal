#include "ResourceManager.h"
#include "StreamFile.h"
#include <algorithm>
#include "SingletonHolder.h"
#include "StreamFileProvider.h"

Pakal::ResourceManager::ResourceManager(void)
{
	//register_reader(new StreamFileProvider());	// memory leak
}

Pakal::ResourceManager::~ResourceManager(void)
{
	m_stream_sources.clear();
}

Pakal::ResourceManager& Pakal::ResourceManager::instance()
{
	static SingletonHolder<ResourceManager> sh;
		return *sh.get();
}

bool Pakal::ResourceManager::add_file_archive(const std::string& path)
{	
	bool result = false;
	auto f = open_resource(path);	
	for( auto& factory : m_stream_sources)
	{		
		auto newFactory = factory->add_file_archive(f);
		if( newFactory )
		{			
			if(( newFactory!= factory) )
				m_stream_sources.push_back( newFactory );
			result = true;
		}
	}
	return result;
}

bool Pakal::ResourceManager::add_data_dir(const std::string& path)
{
	for( auto& factory : m_stream_sources)
	{
		auto newFactory = factory->add_data_dir(path);
		if( newFactory && ( newFactory!= factory))
		{			
			m_stream_sources.push_back( newFactory );
			return true;
		}
	}
	return false;
}

Pakal::IStreamPtr Pakal::ResourceManager::open_resource(const std::string& resourceName)
{
	for( auto& factory : m_stream_sources)
	{
		auto reader = factory->open_reader(resourceName);
		if( reader.get() )
		{
			return reader;
		}
	}
	return nullptr;
}

void Pakal::ResourceManager::register_reader(IFileArchive* factory)
{
	m_stream_sources.push_back(factory);
}

void Pakal::ResourceManager::remove_reader(IFileArchive* factory)
{
	auto f = std::find(m_stream_sources.begin(), m_stream_sources.end() , factory);
	m_stream_sources.erase( f );	
}