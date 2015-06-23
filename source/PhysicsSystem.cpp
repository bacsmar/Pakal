#include "PhysicsSystem.h"
#include "Poco/Thread.h"
#include "Poco/RunnableAdapter.h"


#if PAKAL_USE_BOX2D == 1
	#include "Box2DPhysicsSystem.h"
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
	m_PhysicsThread = new Poco::Thread();
	m_entryPoint = new Poco::RunnableAdapter<PhysicsSystem>(*this, &PhysicsSystem::run);
}

void PhysicsSystem::run()
{
		std::cout << "Hello, world! from Physics" << std::endl;
	while (true)
	{
		this->dispatchTasks(true);
	}
}

void PhysicsSystem::initialize()
{
	m_PhysicsThread->setName("Physics");
	m_PhysicsThread->start(*m_entryPoint);
}

void PhysicsSystem::terminate()
{
	m_PhysicsThread->join();
}