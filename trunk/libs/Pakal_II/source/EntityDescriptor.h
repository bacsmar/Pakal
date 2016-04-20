#pragma once
#include "Config.h"
#include "RTTI.h"
#include <string>

namespace Pakal
{
	class Archive;

	class _PAKALExport EntityDescriptor
	{
		friend class EntityManager;

		DECLARE_RTTI(EntityDescriptor);

		std::string   m_name;
	protected:

		virtual ~EntityDescriptor();

	public:
		explicit EntityDescriptor(const char* name);

		const std::string& get_name() { return m_name; };
		virtual void persist(Archive* a);
	};

	
}
