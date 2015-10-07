#pragma once

#include "Config.h"
#include "IStream.h"
#include "TaskFwd.h"

#include <memory>

namespace Pakal
{
	class _PAKALExport MemoryStream : public IStream
	{
		SharedPtr<char> m_data;
		std::streamoff m_position;
		std::streamoff m_size;
		path m_name;


	public:

		MemoryStream() {}

		explicit MemoryStream(IStream* source) : m_position(0), m_size(source->size()), m_name(source->resource_name())
		{
			m_data = std::shared_ptr<char>(new char[static_cast<unsigned>(source->size())], std::default_delete<char[]>());

			source->seek(0);
			source->read(m_data.get(), m_size);
		}

		SharedPtr<MemoryStream> new_from_buffer()
		{
			auto copy = std::make_shared<MemoryStream>();

			copy->m_position = 0;
			copy->m_data = m_data;
			copy->m_name = m_name;
			copy->m_size = m_size;

			return copy;
		}


		inline std::streamoff read(void* data, std::streamoff size) override
		{
			ASSERT(tell() + size <= m_size);

			const char* startData = m_data.get() + m_position;
			std::memcpy(data, startData, static_cast<size_t>(size));

			return size;
		}

		inline bool seek(std::streamoff position, bool relative) override
		{
			m_position = relative ? m_position + position : position;
			ASSERT(m_position < m_size && m_position > -1);
			return true;
		}
		inline std::streamoff tell() override { return m_position; }
		inline std::streamoff size() override { return m_size; }
		inline const path& resource_name() override { return m_name; }
	};

}