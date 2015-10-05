#pragma once

#include "Config.h"
#include <string>
#include <algorithm>

namespace Pakal
{
	class _PAKALExport path : public std::string
	{		
	public:
		path(const char* str) : std::string(str)
		{
			std::replace(begin(), end(), '\\', '/');
			std::transform(begin(), end(), begin(), ::tolower);
		}

		path(const std::string& str) : path(str.c_str()) {}		
	};
}
