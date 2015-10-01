#pragma once

#include "Config.h"
#include "TaskFwd.h"

#include "IReadFile.h"
#include "IStream.h"


namespace Pakal
{
	class _PAKALExport StreamIrrlitch : public irr::io::IReadFile
	{
		SharedPtr<IStream> m_source;
		irr::io::path m_resource_name;

	public:
		explicit StreamIrrlitch(SharedPtr<IStream> source) : m_source(source)
		{
			m_resource_name = m_source->resource_name().c_str();
		}

		irr::s32 read(void* buffer, irr::u32 sizeToRead) override
		{
			return static_cast<irr::s32>(m_source->read(buffer, sizeToRead));
		}
		bool seek(long finalPos, bool relativeMovement) override
		{
			return m_source->seek(finalPos, relativeMovement);
		}
		long getSize() const override
		{
			return static_cast<long>(m_source->size());
		}
		long getPos() const override
		{
			return static_cast<long>(m_source->tell());
		}
		const irr::io::path& getFileName() const override
		{
			return m_resource_name;
		}
	};

}
