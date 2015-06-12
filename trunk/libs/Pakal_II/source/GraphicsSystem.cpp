#include "GraphicsSystem.h"
#include "Poco/Foundation.h"

using namespace Pakal;

#if PAKAL_USE_IRRLICHT == 1
#include "irrlicht/IrrGraphicsSystem.h"
#endif

GraphicsSystem* GraphicsSystem::createGraphicsSystem()
{
#if PAKAL_USE_IRRLICHT == 1
	return new IrrGraphicsSystem();
#else
	return new GraphicsSystem();
#endif
}

bool GraphicsSystem::init()
{
	return true;
}

void GraphicsSystem::initWindow() {}

void GraphicsSystem::restoreWindow() {}

void GraphicsSystem::destroyWindow() {}

bool GraphicsSystem::update() { return true; }

void GraphicsSystem::beginScene() {}

bool GraphicsSystem::draw()
{
	return true;
}

void GraphicsSystem::endScene() {}

void GraphicsSystem::run()
{
	init();
	initWindow();

	while (true)
	{
#ifdef PAKAL_WIN32_PLATFORM
		MSG msg;
		while (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
#endif
		bool running = update();

		if (msg.message == WM_QUIT || !running)
		{
			// TODO: notificar a los sistemas que hemos terminado 
			break;
		}
	}
}