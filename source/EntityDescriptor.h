#pragma once
#include "Config.h"
#include <string>

namespace Pakal
{
	class Archive;

	class _PAKALExport EntityDescriptor
	{
		std::string m_name;

	public:
		explicit EntityDescriptor(const char* name);
		virtual ~EntityDescriptor();

		const std::string& get_name() { return m_name; };
		virtual void persist(Archive* a);
	};

	
}
