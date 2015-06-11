#include "GraphicsSystem.h"
#include "LogMgr.h"

using namespace Pakal;

#if PAKAL_USE_IRRLICHT == 1
#include "irrlicht/IrrGraphicsSystem.h"
#endif

Pakal::GraphicsSystem* Pakal::GraphicsSystem::createGraphicsSystem()
{
#if PAKAL_USE_IRRLICHT == 1
	return new Pakal::IrrGraphicsSystem();
#endif

	return new Pakal::GraphicsSystem();
}

bool Pakal::GraphicsSystem::init()
{
	return true;
}

void Pakal::GraphicsSystem::initWindow()
{

}

void Pakal::GraphicsSystem::restoreWindow()
{

}

void Pakal::GraphicsSystem::destroyWindow()
{

}

void Pakal::GraphicsSystem::beginScene()
{

}

bool Pakal::GraphicsSystem::draw( float time )
{
	return true;
}

void Pakal::GraphicsSystem::endScene()
{

}