#include "GraphicsSystem.h"
#include "Poco/Foundation.h"

#include "IComponent.h"
#include "components/RenderComponent.h"
#include "Task.h"

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
//////////////////////////////////////////////////////////////////////////
bool GraphicsSystem::update() { return true; }
//////////////////////////////////////////////////////////////////////////
void GraphicsSystem::beginScene() {}
//////////////////////////////////////////////////////////////////////////
bool GraphicsSystem::draw()
{
	return true;
}
//////////////////////////////////////////////////////////////////////////
void GraphicsSystem::endScene() {}
//////////////////////////////////////////////////////////////////////////
bool GraphicsSystem::initialize()
{
	return true;
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
		dispatchTasks();
		bool running = update();

		if (msg.message == WM_QUIT || !running)
		{
			// TODO: notificar a los sistemas que hemos terminado 
			break;
		}
	}
}
//////////////////////////////////////////////////////////////////////////
BasicTaskPtr GraphicsSystem::initComponentAsync(IComponent *c) 
{
	RenderComponent *pComponent = static_cast<RenderComponent*> (c);

	std::function<int()> lambdaInit = [=] (void) { pComponent->onInit(*this); return 0; };

	return getInbox()->pushTask( lambdaInit );
}
//////////////////////////////////////////////////////////////////////////
BasicTaskPtr GraphicsSystem::terminateComponentAsync(IComponent *c) 
{
	RenderComponent *pComponent = static_cast<RenderComponent*> (c);

	std::function<int()> lamdaDestroy = [=] (void) 
	{		
		pComponent->onDestroy(*this);
		delete pComponent;
		return 0;
	};

	return getInbox()->pushTask( lamdaDestroy );
}
//////////////////////////////////////////////////////////////////////////