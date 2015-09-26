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

	template <class T>
	using TaskPtr = std::shared_ptr<Task<T>>;

	template <class T>
	using Ptr = std::shared_ptr<T>;

	const std::thread::id NULL_THREAD = std::thread::id();
	#define  THIS_THREAD  std::this_thread::get_id()

}