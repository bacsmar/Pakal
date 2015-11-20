#pragma once

#include "Config.h"
#include <string>
#include <algorithm>

namespace Pakal
{
	class _PAKALExport Path
	{
		std::string m_path;

	public:
		Path()
		{
		}

		Path(const char* str) : m_path(str)
		{
			std::replace(m_path.begin(), m_path.end(), '\\', '/');
			std::transform(m_path.begin(), m_path.end(), m_path.begin(), ::tolower);
		}
		Path& operator=(const Path& other)
		{
			if (this == &other)
				return *this;
			m_path = other.m_path;
			return *this;
		}
		Path& operator=(Path&& other)
		{
			if (this == &other)
				return *this;
			m_path = std::move(other.m_path);
			return *this;
		}

		Path(const std::string& str) : Path(str.c_str()) {}
		Path(const Path& other) : m_path { other.m_path } {}
		Path(Path&& other) : m_path { std::move(other.m_path) } {}

		operator const std::string&() const { return m_path; }
		operator const char*() const { return m_path.c_str(); }

		const char* c_str() const { return m_path.c_str();}

		inline std::size_t size() { return m_path.size(); }

		bool operator<(const Path& path2) const { return m_path < path2.m_path; }

	};
}
