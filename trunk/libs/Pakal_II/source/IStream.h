#pragma once
#include "Config.h"

#include <ios>

namespace Pakal
{
	
	class _PAKALExport IStream
	{
	public:
		virtual ~IStream() {}

		virtual std::streamoff read(void* data, std::streamoff size) = 0;
		virtual std::streamoff tell() = 0;
		virtual std::streamoff size() = 0;
		virtual bool seek(std::streamoff position, bool relative = false) = 0;
		virtual const std::string& resource_name() = 0;
	};
}