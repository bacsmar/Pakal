#pragma once
#include "Config.h"

#include <memory>
#include <thread>

namespace Pakal
{
	template<class TArgs>
	class _PAKALExport Task;

	class _PAKALExport BasicTask;	
	typedef std::shared_ptr<BasicTask> BasicTaskPtr;

	const std::thread::id NULL_THREAD = std::thread::id();

}