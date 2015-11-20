#pragma once
#include "ISource.h"

namespace Pakal
{
	class _PAKALExport DirectorySource : public ISource
	{
		DECLARE_RTTI_WITH_BASE(DirectorySource, ISource);
	public:		

		virtual bool initialize(const Path& path, bool recursive) = 0;
		virtual void terminate() = 0;
	};

}
