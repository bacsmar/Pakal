#include "TaskCompletionSource.h"

namespace Pakal
{
	TaskCompletionSource::TaskCompletionSource(EventScheduler* scheduler)
	{
		static auto emptyDelegate = [](){ };
		m_task = std::make_shared<BasicTask>(emptyDelegate,scheduler);
	}
}