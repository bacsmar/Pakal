#include "GraphicsSystem.h"
#include "Poco/Foundation.h"

#include "IComponent.h"
#include "EventScheduler.h"

#include "components/GraphicComponent.h"

using namespace Pakal;

#if PAKAL_USE_IRRLICHT == 1
#include "irrlicht/IrrGraphicsSystem.h"
#endif

GraphicsSystem* GraphicsSystem::createInstance()
{
#if PAKAL_USE_IRRLICHT == 1
	return new IrrGraphicsSystem();
#else
	return new NullGraphicSystem();
#endif
}
//////////////////////////////////////////////////////////////////////////
bool GraphicsSystem::initialize()
{
	return onInitialize();
}
//////////////////////////////////////////////////////////////////////////
void GraphicsSystem::run()
{

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
		// dispatch all task, to fill the two below lists
		dispatchTasks();

		bool running = update();

		if (msg.message == WM_QUIT || !running)
		{
			// TODO: notificar a los sistemas que hemos terminado 
			break;
		}
	}
}


BasicTaskPtr GraphicsSystem::initComponentAsync(IComponent* c)
{
	return getScheduler()->executeInThread([c]()
	{
		GraphicComponent *pComponent = static_cast<GraphicComponent*> (c);
		pComponent->onInit();
	},this->threadId());
}

BasicTaskPtr GraphicsSystem::terminateComponentAsync(IComponent* c)
{
	return getScheduler()->executeInThread([c]()
	{
		GraphicComponent *pComponent = static_cast<GraphicComponent*> (c);
		pComponent->onDestroy();
	},this->threadId());
}