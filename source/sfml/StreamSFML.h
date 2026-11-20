#pragma once
#include "Config.h"
#include "TaskFwd.h"
#include "Utils.h"
#include "SFML/System/InputStream.hpp"


namespace Pakal
{
	class StreamSFML : public sf::InputStream
	{
		SharedPtr<std::istream> m_source;

	public:
		explicit StreamSFML(SharedPtr<std::istream> stream) : m_source(stream) {}

		sf::Int64 read(void* data, sf::Int64 size) override
		{
			m_source->read(static_cast<char*>(data), size);
			const sf::Int64 count = m_source->gcount();
			m_source->clear();   // a short read at the end sets eof/fail: tell() would then be -1 and a decoder (Vorbis) can loop forever

			return count;
		}
		sf::Int64 seek(sf::Int64 position) override
		{
			m_source->clear();

			m_source->seekg(position, m_source->beg);

			return m_source->tellg();
		}
		sf::Int64 tell() override
		{
			m_source->clear();
			return m_source->tellg();
		}
		sf::Int64 getSize() override
		{
			m_source->clear();
			return static_cast<sf::Int64>(file_utils::stream_size(*m_source));
		}

	};
}
