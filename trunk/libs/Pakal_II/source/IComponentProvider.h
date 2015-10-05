#pragma once

#include "Config.h"
#include <vector>
#include "ComponentFactory.h"

namespace Pakal
{
	class _PAKALExport IComponentProvider
	{
	public:
		virtual ~IComponentProvider(){}

		virtual void register_component_factories(std::vector<IComponentFactory*>& factories) = 0;
	};
}