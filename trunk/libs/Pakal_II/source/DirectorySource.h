#pragma once
#include "ISource.h"

namespace Pakal
{
	class _PAKALExport DirectorySource : public ISource
	{
	public:
		DECLARE_RTTI_WITH_BASE(DirectorySource, ISource);

		virtual bool initialize(const std::string& path, bool recursive) = 0;
		virtual void terminate() = 0;
	};

}
