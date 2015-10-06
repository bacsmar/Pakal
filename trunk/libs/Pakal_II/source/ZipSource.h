#pragma once
#include "ISource.h"

namespace Pakal
{
	class _PAKALExport ZipSource : public ISource
	{
		DECLARE_RTTI_WITH_BASE(ZipSource, ISource);
	public:

		virtual bool initialize(const path& pathToZip, bool recursiveDirs, const std::string& password = "") = 0;
		virtual void terminate() = 0;
	};

}
