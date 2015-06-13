#pragma once

#include "Config.h"
#include <vector>

namespace Pakal
{

	class IComponentFactory;

	class _PAKALExport IComponentProvider
	{
	public:	
		~IComponentProvider(){}

		virtual void registerComponentFactories( std::vector<IComponentFactory*> &factories) = 0;
	};
}