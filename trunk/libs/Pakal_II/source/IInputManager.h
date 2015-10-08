#pragma once

#include "Config.h"
#include "IManager.h"
#include "IComponentProvider.h"

namespace Pakal
{
	class _PAKALExport IInputManager : public IManager, public IComponentProvider
	{
	public:
		virtual ~IInputManager(){};
	};
}