#pragma once

#include "Config.h"
#include "RTTI.h"
#include "TaskFwd.h"
#include "IStream.h"
#include "path.h"

namespace Pakal
{

	class _PAKALExport ISource
	{
	public:
		DECLARE_RTTI(ISource);

		virtual ~ISource() {}

		virtual SharedPtr<IStream> open_resource(const path& pathToResource) = 0;
	};
}
