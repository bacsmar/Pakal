#pragma once

#include "Config.h"
#include <vector>

namespace Pakal
{

	class IComponentFactory;
	class BasicTask;
	class Component;

	class _PAKALExport IComponentProvider
	{
	public:
		virtual ~IComponentProvider(){}

		virtual void register_component_factories(std::vector<IComponentFactory*>& factories) = 0;
	};
}