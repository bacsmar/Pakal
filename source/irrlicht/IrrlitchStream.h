#pragma once

#include "Config.h"
#include "IReadFile.h"
#include <istream>

namespace Pakal
{
	class _PAKALExport IrrlitchStream : public std::istream
	{
		class irrlitch_buff : public std::streambuf
		{

			irr::io::IReadFile* m_source;

		protected:			
	
			int_type underflow() override
			{
				ASSERT_MSG(false, "operation not supported by irritch engine, pay me money to make the pakal version which will support this");
				return traits_type::eof();
			}
			std::streamsize xsgetn(char* _Ptr, std::streamsize _Count) override
			{
				return m_source->read(_Ptr, static_cast<irr::u32>(_Count));;
			}
			pos_type seekoff(off_type off, ios_base::seekdir dir , ios_base::openmode which) override
			{
				if (dir == beg)
				{
					m_source->seek(static_cast<long>(off), false);
				}
				else if (dir == cur)
				{
					m_source->seek(static_cast<long>(off), true);
				}
				else if (dir == end)
				{
					m_source->seek( m_source->getSize() -  static_cast<long>(off), false);
				}

				return m_source->getPos();
			}
			pos_type seekpos(pos_type pos, ios_base::openmode which) override
			{
				return seekoff(pos, beg, which);
			}

		public:

			irrlitch_buff(irr::io::IReadFile* source) : m_source(source) {}
			~irrlitch_buff() { m_source->drop(); }

		};

		irrlitch_buff m_buff;

	public:
		explicit IrrlitchStream(irr::io::IReadFile* source) : std::istream(&m_buff), m_buff(source) {}
	};
}
