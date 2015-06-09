#pragma once
#include "Config.h"

namespace Pakal
{

	class IPakalApplication;

	class _PAKALExport Engine 
	{
	public:
		void run(IPakalApplication *application );

		static Engine &instance();
	protected:
		IPakalApplication * mApplication;
	};
}