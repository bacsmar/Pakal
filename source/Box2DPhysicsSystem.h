#pragma once

#include "PhysicsSystem.h"

namespace Pakal
{
	class Engine;

	class _PAKALExport Box2DPhysicsSystem : public PhysicsSystem
	{
	public:	
		virtual ~Box2DPhysicsSystem();
		Box2DPhysicsSystem();
	protected:

		friend class Engine;		

		virtual void registerComponentFactories( std::vector<IComponentFactory*> &factories);
		virtual void run();
	};
}