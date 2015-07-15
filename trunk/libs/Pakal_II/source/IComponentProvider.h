#pragma once

#include "Config.h"
#include <vector>
#include "TaskFwd.h"

namespace Pakal
{

	class IComponentFactory;
	class BasicTask;
	class IComponent;

	class _PAKALExport IComponentProvider
	{
	public:
		virtual ~IComponentProvider(){}

		virtual void registerComponentFactories( std::vector<IComponentFactory*> &factories) = 0;
		virtual BasicTaskPtr initComponentAsync(IComponent *c) = 0;
		virtual BasicTaskPtr terminateComponentAsync(IComponent *c) = 0;
	};
}