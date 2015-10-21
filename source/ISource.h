#pragma once

#include "Config.h"
#include "RTTI.h"
#include "TaskFwd.h"
#include "path.h"

namespace Pakal
{

	class _PAKALExport ISource
	{
		DECLARE_RTTI(ISource);
	public:

		virtual ~ISource() {}

		virtual SharedPtr<std::istream> open_resource(const path& pathToResource) = 0;
	};
}
