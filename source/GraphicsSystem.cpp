#include "GraphicsSystem.h"
#include "Poco/Foundation.h"

#include "IComponent.h"
#include "components/RenderComponent.h"
#include "Task.h"
#include "InboxQueue.h"


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
	this->dispatchTasks();
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
		// then process the filled lists
		onProcessComponentUpdateList(m_updateList);

		bool running = update();

		if (msg.message == WM_QUIT || !running)
		{
			// TODO: notificar a los sistemas que hemos terminado 
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
BasicTaskPtr GraphicsSystem::addToUpdateList(RenderComponent *c)
{
	// TODO: 
	std::function<int()> lambda = [=] (void) 
	{ 
		this->m_updateList.insert(c);
		return 0;
	};

	return getInbox()->pushTask( lambda );
}
//////////////////////////////////////////////////////////////////////////
BasicTaskPtr GraphicsSystem::initComponentAsync(IComponent *c) 
{	
	std::function<void()> lambdaInit = [=] (void) 
	{ 
		RenderComponent *pComponent = static_cast<RenderComponent*> (c);		
		onInitComponent( pComponent );
	};
	InboxQueue *inbox = getInbox();
	return inbox->pushTask( lambdaInit );
}
//////////////////////////////////////////////////////////////////////////
BasicTaskPtr GraphicsSystem::terminateComponentAsync(IComponent *c) 
{	
	std::function<int()> lamdaDestroy = [=] (void) 
	{
		RenderComponent *pComponent = static_cast<RenderComponent*> (c);
		this->m_updateList.erase(pComponent);
		pComponent->onDestroy(*this);
		delete pComponent;
		return 0;
	};

	return getInbox()->pushTask( lamdaDestroy );
}
//////////////////////////////////////////////////////////////////////////