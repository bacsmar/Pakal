#pragma once
#include "ISource.h"

namespace Pakal
{
	class _PAKALExport ZipSource : public ISource
	{
	public:
		DECLARE_RTTI_WITH_BASE(ZipSource, ISource);

		virtual bool initialize(const std::string& pathToZip, bool recursiveDirs, const std::string& password) = 0;
		virtual void terminate() = 0;
	};

}
