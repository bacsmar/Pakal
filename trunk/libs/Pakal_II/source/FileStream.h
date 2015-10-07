#pragma once
#include "Config.h"
#include "IStream.h"

#include <fstream>
#include <string>


namespace Pakal
{
	class _PAKALExport FileStream : public IStream
	{
		std::fstream m_stream;
		path m_path;

	public:
		FileStream(const path& path, WriteMode mode) : m_path(path.c_str())
		{
			int flags = 0;

			flags |= enum_utils::has_flag(mode, WriteMode::Append) ? std::fstream::app : 0;
			flags |= enum_utils::has_flag(mode, WriteMode::AtEnd) ? std::fstream::ate : 0;
			flags |= enum_utils::has_flag(mode, WriteMode::Binary) ? std::fstream::binary : 0;
			flags |= enum_utils::has_flag(mode, WriteMode::Truncate) ? std::fstream::trunc : 0;
			flags |= enum_utils::has_flag(mode, WriteMode::__ReadOnly) ? std::fstream::in : std::fstream::out;

			m_stream.open(path.c_str(), flags);
		}

		bool is_open()
		{
			return m_stream.is_open();
		}


		~FileStream() override
		{
			if (m_stream.is_open())
				m_stream.close();
		}
		std::streamoff read(void* data, std::streamoff size) override
		{
			auto startPos = tell();

			m_stream.read(static_cast<char*>(data), size);

			auto count = m_stream.gcount();
			
			if (!m_stream.good())
			{
				m_stream.clear();
				m_stream.seekg(startPos + count, m_stream.beg);
			}

			m_stream.seekp(m_stream.tellg(), m_stream.beg);

			return count;
		}
		std::streamoff write(void* data, std::streamoff size) override
		{
			auto startPos = tell();

			m_stream.write(static_cast<char*>(data), size);

			auto count = m_stream.good() ? size : -1;

			if (!m_stream.good())
			{
				m_stream.clear();
				m_stream.seekp(startPos, m_stream.beg);
			}

			m_stream.seekg(m_stream.tellp(), m_stream.beg);

			return count;
		}
		bool seek(std::streamoff position, bool relative) override
		{
			m_stream.seekg(position, relative ? m_stream.cur : m_stream.beg);
			m_stream.seekp(position, relative ? m_stream.cur : m_stream.beg);

			return m_stream.good();
		}

		std::streamoff size() override
		{
			auto curr = m_stream.tellg();
			m_stream.seekg(0, m_stream.end);
			std::streamoff size = m_stream.tellg();
			m_stream.seekg(curr, m_stream.beg);

			return size;
		}
		std::streamoff tell() override
		{
			ASSERT(m_stream.tellg() == m_stream.tellp());
			return m_stream.tellg();
		}
		const path& resource_name() override
		{
			return m_path;
		}
	};
}