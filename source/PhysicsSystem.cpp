#include "PhysicsSystem.h"
#include "Poco/Thread.h"
#include "Poco/RunnableAdapter.h"

#include "IComponent.h"
#include "components/PhysicComponent.h"

#include "Task.h"
#include "InboxQueue.h"


#if PAKAL_USE_BOX2D == 1
	#include "Box2D/Box2DPhysicsSystem.h"
#endif

using namespace Pakal;

PhysicsSystem* PhysicsSystem::createPhysicsSystem()
{
#if PAKAL_USE_BOX2D == 1
	return new Box2DPhysicsSystem();
#else
	return new PhysicsSystem();
#endif

}

PhysicsSystem::~PhysicsSystem()
{
	SAFE_DEL(m_PhysicsThread);
	SAFE_DEL(m_entryPoint);
}

PhysicsSystem::PhysicsSystem()
{
	m_State = SE_STARTING;
	m_PhysicsThread = new Poco::Thread();
	m_entryPoint = new Poco::RunnableAdapter<PhysicsSystem>(*this, &PhysicsSystem::run);
}

void PhysicsSystem::run()
{

	initWorld();
	std::cout << "Hello, world! from Physics" << std::endl;
	m_State = SE_RUNNING;

	while (true)
	{
		dispatchTasks();
		update();
		if( SE_WAITING_STOP == m_State )
		{
			m_State  = SE_STOPING;
			break;
		}
	}
}

void PhysicsSystem::initialize()
{
	m_PhysicsThread->setName("Physics");
	m_PhysicsThread->start(*m_entryPoint);
}

void PhysicsSystem::terminate()
{
	m_State = SE_WAITING_STOP;
	m_PhysicsThread->join();
}

//////////////////////////////////////////////////////////////////////////
BasicTaskPtr PhysicsSystem::initComponentAsync(IComponent *c) 
{
	PhysicComponent *pComponent = static_cast<PhysicComponent*> (c);

	std::function<int()> lambdaInit = [=] (void) { pComponent->onInit(*this); return 0; };

	return getInbox()->pushTask( lambdaInit );
}
//////////////////////////////////////////////////////////////////////////
BasicTaskPtr PhysicsSystem::terminateComponentAsync(IComponent *c) 
{
	PhysicComponent *pComponent = static_cast<PhysicComponent*> (c);

	std::function<int()> lamdaDestroy = [=] (void) 
	{		
		//m_updateList.erase(pComponent);
		pComponent->onDestroy(*this);
		delete pComponent;
		return 0;
	};

	return getInbox()->pushTask( lamdaDestroy );
}