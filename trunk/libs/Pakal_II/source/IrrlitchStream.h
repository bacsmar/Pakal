#pragma once

#include "Config.h"
#include "IStream.h"
#include "IReadFile.h"

namespace Pakal
{
	class _PAKALExport IrrlitchStream : public IStream
	{
		irr::io::IReadFile* m_source;
		std::string m_resource_name;

	public:
		explicit IrrlitchStream(irr::io::IReadFile* source) : m_source(source)
		{
			m_resource_name = m_source->getFileName().c_str();
		}

		std::streamoff read(void* data, std::streamoff size) override
		{
			return m_source->read(data, static_cast<irr::u32>(size));
		}

		std::streamoff tell() override
		{
			return m_source->getPos();
		}
		std::streamoff size() override
		{
			return m_source->getSize();
		}
		bool seek(std::streamoff position, bool relative) override
		{
			return m_source->seek(static_cast<long>(position), relative);
		}
		const std::string& resource_name() override
		{
			return m_resource_name;
		}

		~IrrlitchStream()
		{
			m_source->drop();
		}
	};
}
