#include "TaskCompletionSource.h"

namespace Pakal
{
	TaskCompletionSource::TaskCompletionSource()
	{
		static auto emptyDelegate = [](){ };
		m_task = std::make_shared<BasicTask>(emptyDelegate);
	}
}