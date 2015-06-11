#pragma once
#include "Config.h"

namespace Pakal
{
	
	class _PAKALExport PhysicsSystem
	{
	public:

		virtual void run();

	protected:
		friend class Engine;
		static PhysicsSystem* createPhysicsSystem();
		virtual ~PhysicsSystem(){}
	};
}