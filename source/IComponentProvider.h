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

		virtual void registerYourComponents( std::vector<IComponentFactory*> &componentVector) = 0;
	};
}