#include "GraphicsSystem.h"
#include "LogMgr.h"

using namespace Pakal;

#if PAKAL_USE_IRRLICHT == 1
	#include "irrlicht/IrrGraphicsSystem.h"
#endif

	Pakal::GraphicsSystem* Pakal::GraphicsSystem::createGraphicsSystem()
	{
		GraphicsSystem *graphicsSystem = nullptr;

	#if PAKAL_USE_IRRLICHT == 1
		return new Pakal::IrrGraphicsSystem();
	#endif
		
		return new Pakal::GraphicsSystem();
	}