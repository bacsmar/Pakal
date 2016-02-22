#include "rocketgui/PakalRocketFileInterface.h"

#include "ResourceManager.h"
#include <ios>

using namespace Pakal;

PakalRocketFileInterface::PakalRocketFileInterface() 
{
}

PakalRocketFileInterface::~PakalRocketFileInterface()
{
}

Rocket::Core::FileHandle PakalRocketFileInterface::Open(const Rocket::Core::String& path)
{	
	auto file = ResourceManager::instance().open_read_resource(path.CString(), false);

	if(file)
	{
		auto index = reinterpret_cast<Rocket::Core::FileHandle>(file.get());

		m_opened_files[index] = file;
		return index;
	}
	return 0;
}

void PakalRocketFileInterface::Close(Rocket::Core::FileHandle file)
{
	const auto &f = m_opened_files.find(file);
	if( f != m_opened_files.end() )
	{
		m_opened_files.erase(f);
	}
}

size_t PakalRocketFileInterface::Read(void* buffer, size_t size, Rocket::Core::FileHandle file)
{
	ASSERT(file != 0);

	auto f = reinterpret_cast<std::istream*>(file);
	
	f->read(static_cast<char*>(buffer), size);

	return static_cast<size_t>(f->gcount());
}

bool PakalRocketFileInterface::Seek(Rocket::Core::FileHandle file, long offset, int origin)
{
	ASSERT(file != 0);

	std::istream* rfile = reinterpret_cast<std::istream*>(file);	
	//enum seekdir {beg, cur, end};
	std::ios::seekdir dir = std::ios::beg;

	switch (origin)
	{
	case SEEK_SET: dir = std::ios::beg; break;	
	case SEEK_CUR: dir = std::ios::cur; break;
	case SEEK_END: dir = std::ios::end; break;
	}

	rfile->seekg(offset, dir);

	return rfile->good();
}

size_t PakalRocketFileInterface::Tell(Rocket::Core::FileHandle file)
{
	ASSERT(file != 0);

	auto f = reinterpret_cast<std::istream*>(file);
	
	return static_cast<size_t>(f->tellg());
}
