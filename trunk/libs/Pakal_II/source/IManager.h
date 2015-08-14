#pragma once
#include "Config.h"

namespace  Pakal
{
	class _PAKALExport IManager
	{
	public:
		virtual ~IManager() { }

		virtual void initialize() = 0;
		virtual void terminate() = 0;
	};
}