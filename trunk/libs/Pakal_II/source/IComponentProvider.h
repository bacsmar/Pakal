#pragma once

#include "Config.h"
#include <vector>

namespace Pakal
{

	class IComponentFactory;
	class BasicTask;
	class IComponent;

	class _PAKALExport IComponentProvider
	{
	public:	
		~IComponentProvider(){}

		virtual void registerComponentFactories( std::vector<IComponentFactory*> &factories) = 0;
		virtual BasicTask * initComponentAsync(IComponent *c) = 0;
		virtual BasicTask * terminateComponentAsync(IComponent *c) = 0;
	};
}