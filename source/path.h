#pragma once

#include "Config.h"
#include <string>
#include <algorithm>

namespace Pakal
{
	class _PAKALExport path
	{
		std::string m_path;

	public:
		path()
		{
		}

		path(const char* str) : m_path(str)
		{
			std::replace(m_path.begin(), m_path.end(), '\\', '/');
			std::transform(m_path.begin(), m_path.end(), m_path.begin(), ::tolower);
		}
		path& operator=(const path& other)
		{
			if (this == &other)
				return *this;
			m_path = other.m_path;
			return *this;
		}
		path& operator=(path&& other)
		{
			if (this == &other)
				return *this;
			m_path = std::move(other.m_path);
			return *this;
		}

		path(const std::string& str) : path(str.c_str()) {}
		path(const path& other) : m_path { other.m_path } {}
		path(path&& other) : m_path { std::move(other.m_path) } {}

		operator const std::string&() const { return m_path; }
		operator const char*() const { return m_path.c_str(); }

		const char* c_str() const { return m_path.c_str();}

		inline std::size_t size() { return m_path.size(); }

		bool operator<(const path& path2) const { return m_path < path2.m_path; }

	};
}
