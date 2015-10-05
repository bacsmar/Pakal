#pragma once
#include "Config.h"
#include "TaskFwd.h"
#include "IStream.h"
#include "SFML/System/InputStream.hpp"


namespace Pakal
{
	class _PAKALExport StreamSFML : public sf::InputStream
	{
		SharedPtr<IStream> m_source;

	public:
		explicit StreamSFML(SharedPtr<IStream> stream) : m_source(stream) {}

		sf::Int64 read(void* data, sf::Int64 size) override
		{
			return m_source->read(data, static_cast<std::streamoff>(size));
		}
		sf::Int64 seek(sf::Int64 position) override
		{
			return m_source->seek(static_cast<std::streamoff>(position)) ? position : -1;
		}
		sf::Int64 tell() override
		{
			return m_source->tell();
		}
		sf::Int64 getSize() override
		{
			return m_source->size();
		}

	};
}
