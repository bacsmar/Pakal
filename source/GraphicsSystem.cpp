#include "GraphicsSystem.h"
#include "Poco/Foundation.h"

#include "IComponent.h"
#include "components/GraphicComponent.h"
#include "BasicTask.h"
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
	return new NullGraphicSystem();
#endif
}
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
		updateComponents();

		bool running = update();

		if (msg.message == WM_QUIT || !running)
		{
			// TODO: notificar a los sistemas que hemos terminado 
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void GraphicsSystem::addToUpdateList(GraphicComponent *c)
{
	m_updateList.insert(c);
}
//////////////////////////////////////////////////////////////////////////
BasicTaskPtr GraphicsSystem::initComponentAsync(IComponent *c) 
{	
	std::function<void()> lambdaInit = [=] (void) 
	{ 
		GraphicComponent *pComponent = static_cast<GraphicComponent*> (c);
		pComponent->onInit(*this);
	};

	return getInbox()->pushTask( lambdaInit );
}
//////////////////////////////////////////////////////////////////////////
BasicTaskPtr GraphicsSystem::terminateComponentAsync(IComponent *c) 
{	
	std::function<void()> lamdaDestroy = [=] (void) 
	{
		GraphicComponent *pComponent = static_cast<GraphicComponent*> (c);

		m_updateList.erase(pComponent);		
		pComponent->onDestroy(*this);
		delete pComponent; //te odioo
	};

	return getInbox()->pushTask( lamdaDestroy );
}
//////////////////////////////////////////////////////////////////////////

void GraphicsSystem::updateComponents()
{
	for (GraphicComponent* c : m_updateList.getListToProcess() )
	{
		c->onUpdate();
	}
}

//////////////////////////////////////////////////////////////////////////