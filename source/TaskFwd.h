#pragma once
#include "Config.h"

#include <memory>
#include <thread>
#include <mutex>

namespace Pakal
{
	class _PAKALExport BasicTask;

	template<class TArgs> class _PAKALExport Task;

	using BasicTaskPtr = std::shared_ptr<BasicTask>;

	template <class T>
	using TaskPtr = std::shared_ptr<Task<T>>;

	template <class T>
	using SharedPtr = std::shared_ptr<T>;

	template <class T>
	using UniquePtr = std::unique_ptr<T>;
	
	template <class T>
	using WeakPtr = std::weak_ptr<T>;

	using mutex_guard = std::lock_guard<std::mutex>;

	const std::thread::id NULL_THREAD = std::thread::id();
	#define  THIS_THREAD  std::this_thread::get_id()
	#define nameof(x) #x

}