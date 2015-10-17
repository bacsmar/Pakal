#include "rocketgui/PakalRocketFileInterface.h"

#include "ResourceManager.h"

using namespace Pakal;

PakalRocketFileInterface::PakalRocketFileInterface() : m_file_system(&ResourceManager::instance())
{
}

PakalRocketFileInterface::~PakalRocketFileInterface()
{
}

Rocket::Core::FileHandle PakalRocketFileInterface::Open(const Rocket::Core::String& path)
{	
	auto file = m_file_system->open_read_resource(path.CString(), false);
	if( file.get() )
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
	auto f = reinterpret_cast<Pakal::IStream*>(file);
	return static_cast<size_t>(f->read(buffer, size));
}

bool PakalRocketFileInterface::Seek(Rocket::Core::FileHandle file, long offset, int origin)
{
	ASSERT(file != 0);
	auto rfile = reinterpret_cast<Pakal::IStream*>(file);


	if (origin == SEEK_SET)
		return rfile->seek(offset, false);

	if (origin == SEEK_CUR)
		return rfile->seek(offset, true);	
	// SEEK_END
	rfile->seek(rfile->size(), false);
	return rfile->seek(offset, true);	
}

size_t PakalRocketFileInterface::Tell(Rocket::Core::FileHandle file)
{
	ASSERT(file != 0);
	auto f = reinterpret_cast<Pakal::IStream*>(file);
	return static_cast<size_t>(f->tell());
}
