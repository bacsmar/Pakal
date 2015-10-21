#pragma once

#include "Config.h"
#include "TaskFwd.h"
#include "Utils.h"

#include <memory>
#include <strstream>

namespace Pakal
{
	class _PAKALExport memory_istream : public std::istream
	{
		SharedPtr<std::string> m_data;
		std::strstreambuf m_buffer;

		memory_istream(const memory_istream& other) = delete;
		memory_istream& operator=(const memory_istream& other) = delete;

	public:
		static SharedPtr<memory_istream> from_istream(std::istream* source)
		{
			auto originalPos = source->tellg();
			auto size = file_utils::stream_size(*source);
			
			source->seekg(0, beg);

			auto ptr = std::make_shared<memory_istream>(std::make_shared<std::string>(static_cast<unsigned>(size),'0'));

			source->read(&ptr->m_data->at(0), size);

			source->seekg(originalPos, beg);

			return ptr;
		}
		static SharedPtr<memory_istream> from_buffer(SharedPtr<memory_istream>& buffer)
		{
			return std::make_shared<memory_istream>(buffer->m_data);
		}

		explicit memory_istream(SharedPtr<std::string> data) : 
			std::istream(&m_buffer), 
			m_data(data), 
			m_buffer(std::strstreambuf(m_data->c_str(), m_data->size()))
		{}

	};

}