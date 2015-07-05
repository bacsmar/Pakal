#pragma once
#include "Config.h"

#include <memory>

namespace Pakal
{
	template<class TArgs>
	class _PAKALExport Task;

	class BasicTask;	
	typedef std::shared_ptr<BasicTask> BasicTaskPtr;
}