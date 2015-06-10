#pragma once
#include "Config.h"

namespace Pakal
{
	
	class _PAKALExport GraphicsSystem
	{
		friend class Engine;
	protected:
		virtual ~GraphicsSystem(){}

		static GraphicsSystem* createGraphicsSystem();

	};
}