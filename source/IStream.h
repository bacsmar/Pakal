#pragma once
#include "Config.h"
#include "Utils.h"
#include "path.h"

namespace Pakal
{

	enum class WriteMode
	{
		None	 = 0,
		AtEnd	 = 1 << 0,
		Binary	 = 1 << 1,
		Append	 = 1 << 2,
		Truncate = 1 << 3,
		__ReadOnly	 = 1 << 4,
	};
	BIT_ENUM(WriteMode)
	
	class _PAKALExport IStream
	{
	public:
		virtual ~IStream() {}

		virtual std::streamoff read(void* data, std::streamoff size) = 0;
		virtual std::streamoff write(void* data, std::streamoff size) { ASSERT_MSG(false, "stream must be opened in write mode"); return -1; }
		virtual std::streamoff tell() = 0;
		virtual std::streamoff size() = 0;
		virtual bool seek(std::streamoff position, bool relative = false) = 0;
		virtual const path& resource_name() = 0;
	};
}