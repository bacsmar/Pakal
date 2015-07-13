#include "GraphicsSystem.h"
#include "Poco/Foundation.h"

#include "IComponent.h"
#include "components/GraphicComponent.h"
#include "Task.h"
#include "InboxQueue.h"
#include <mutex>

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

		{
			//std::lock_guard<std::mutex> lock( m_UpdateMutex );
			for( auto component : m_updateList)
			{
				
			}
		}
		// then process the filled lists
		//onProcessComponentUpdateList(m_updateList);

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
	//std::lock_guard<std::mutex> lock( m_UpdateMutex ); 
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
	std::function<int()> lamdaDestroy = [=] (void) 
	{
		GraphicComponent *pComponent = static_cast<GraphicComponent*> (c);

		this->m_updateList.erase(pComponent);		
		pComponent->onDestroy(*this);
		delete pComponent;
		return 0;
	};

	return getInbox()->pushTask( lamdaDestroy );
}
//////////////////////////////////////////////////////////////////////////