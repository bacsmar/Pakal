#pragma once

#include "Config.h"
#include "RTTI.h"
#include "TaskFwd.h"
#include "IStream.h"

namespace Pakal
{

	class _PAKALExport ISource
	{
	public:
		DECLARE_RTTI(ISource);

		virtual ~ISource() {}

		virtual SharedPtr<IStream> open_resource(const std::string& pathToResource) = 0;
	};
}
