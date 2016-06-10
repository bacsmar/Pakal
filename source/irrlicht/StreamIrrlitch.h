#pragma once

#include "Config.h"
#include "TaskFwd.h"

#include "IReadFile.h"
#include <istream>


namespace Pakal
{
	class _PAKALExport StreamIrrlitch : public irr::io::IReadFile
	{
		SharedPtr<std::istream> m_source;

	public:
		explicit StreamIrrlitch(SharedPtr<std::istream> source) : m_source(source)
		{
		}

		irr::s32 read(void* buffer, irr::u32 sizeToRead) override
		{
			m_source->read(static_cast<char*>(buffer), sizeToRead);

			return (irr::s32)m_source->gcount();
		}
		bool seek(long finalPos, bool relativeMovement) override
		{
			m_source->seekg(finalPos, relativeMovement ? m_source->cur : m_source->beg);

			return m_source->good();
		}
		long getSize() const override
		{
			auto curr = m_source->tellg();

			m_source->seekg(0, m_source->end);
			long pos = (long)m_source->tellg();

			m_source->seekg(curr, m_source->beg);

			return pos;
		}
		long getPos() const override
		{
			return (long)m_source->tellg();
		}
		const irr::io::path& getFileName() const override
		{
			ASSERT_MSG(false, "esperaba no llegar a esto, creo que tendre que hacer una pequena modificacion al resourcemanager");
			static irr::io::path blank;
			return blank;
		}
	};

}
