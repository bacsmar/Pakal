#pragma once

#include "Config.h"
#include "RTTI.h"
#include "TaskFwd.h"
#include "PakalPath.h"

namespace Pakal
{

	class _PAKALExport ISource
	{
		DECLARE_RTTI(ISource);
	public:

		virtual ~ISource() {}

		virtual SharedPtr<std::istream> open_resource(const Path& pathToResource) = 0;
		// Whether it has the file, without reading it (a source that can tell cheaply says so; the others open it)
		virtual bool has_resource(const Path& pathToResource) { return open_resource(pathToResource) != nullptr; }
	};
}
