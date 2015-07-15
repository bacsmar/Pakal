#pragma once
#include "Config.h"

#include <memory>
#include <thread>

namespace Pakal
{
	template<class TArgs>
	class _PAKALExport Task;

	class BasicTask;	
	typedef std::shared_ptr<BasicTask> BasicTaskPtr;

	const std::thread::id NULL_THREAD = std::thread::id();
}