#pragma once
#include "Config.h"
#include "TaskFwd.h"
#include "Utils.h"
#include "SFML/System/InputStream.hpp"


namespace Pakal
{
	class _PAKALExport StreamSFML : public sf::InputStream
	{
		SharedPtr<std::istream> m_source;

	public:
		explicit StreamSFML(SharedPtr<std::istream> stream) : m_source(stream) {}

		sf::Int64 read(void* data, sf::Int64 size) override
		{
			m_source->read(static_cast<char*>(data), size);

			return m_source->gcount();
		}
		sf::Int64 seek(sf::Int64 position) override
		{
			m_source->seekg(position, m_source->beg);

			return m_source->gcount();
		}
		sf::Int64 tell() override
		{
			return m_source->tellg();
		}
		sf::Int64 getSize() override
		{
			return static_cast<sf::Int64>(file_utils::stream_size(*m_source));
		}

	};
}
