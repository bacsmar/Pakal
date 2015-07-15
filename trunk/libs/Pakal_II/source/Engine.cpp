#include "Engine.h"
#include "LogMgr.h"
#include "GraphicsSystem.h"
#include "PhysicsSystem.h"
#include "GameStateSystem.h"
#include "IPakalApplication.h"
#include "EventScheduler.h"
#include "EntitySystem.h"

#include "ComponentSystem.h"

#include "Poco/SingletonHolder.h"
#include "Poco/Thread.h"
#include "Poco/RunnableAdapter.h"

#include <chrono>
#include <thread>

//#include <vld.h>

//////////////////////////// BEGIN TESTS /////////////////////////////////


//////////////////////////// END TESTS /////////////////////////////////

using namespace Pakal;

//////////////////////////////////////////////////////////////////////////
bool Engine::ms_Initialized = false;
//////////////////////////////////////////////////////////////////////////
void Engine::run()
{
	// remember! it is running on a secondary thread, is NOT on main thread
	std::chrono::milliseconds duration( 1 );    
	while( false == m_shouldTerminate )
	{
		m_logicDispatcher->dispatchTasks();
		m_EntitySystem->updateSimulation();
//		m_GameStateSystem->peek_state();
		std::this_thread::sleep_for( duration );
	}
	//	m_GameStateSystem->close();  TODO
}
//////////////////////////////////////////////////////////////////////////
void Engine::init()
{
	ASSERT(ms_Initialized == false);

	//std::cout << "Hello, world! from engine" << std::endl;

	//because salvador is working with tasks in setup game states
	m_logicDispatcher->dispatchTasks();

	m_Application->setUpComponents(m_ComponentSystem);
	m_Application->setUpGameStates(m_GameStateSystem);	

	ms_Initialized = true;

	run();
}
//////////////////////////////////////////////////////////////////////////
void Engine::start( IPakalApplication *application )
{
	LogMgr::init();
	LogMgr::setLogLevel(10);
	LOG_INFO("Initializing Pakal Engine Version " PAKAL_VERSION_NAME);

	m_Application = application;

	
	m_logicDispatcher   = new AsyncTaskDispatcher();
	m_GraphicsSystem	= GraphicsSystem::createInstance();
	m_PhysicsSystem		= PhysicsSystem::createPhysicsSystem();
	m_EventScheduler	= new EventScheduler();
	m_GameStateSystem	= new GameStateSystem();
	m_ComponentSystem	= new ComponentSystem();
	m_EntitySystem		= new EntitySystem();

	m_ComponentSystem->registerFactories(*m_GraphicsSystem);
	m_ComponentSystem->registerFactories(*m_PhysicsSystem);	

	m_EventScheduler->registerDispatcher(m_GraphicsSystem);
	m_EventScheduler->registerDispatcher(m_PhysicsSystem);
	m_EventScheduler->registerDispatcher(m_logicDispatcher);

	m_GameStateSystem->initialize(this);	// executed in diferent thread
	m_PhysicsSystem->initialize();			// it creates his own thread

	m_GraphicsSystem->initialize();			// it uses this very thread
	m_GraphicsSystem->dispatchTasks();      //have ready the inbox and the id

	Poco::RunnableAdapter<Engine> logic_entry_point(*this, &Engine::init);
	m_LogicThread->setName("Logic");
	m_LogicThread->start(logic_entry_point);
	
	//m_GraphicsSystem->addDebugDrawerClient( m_PhysicsSystem->getDebugDrawer() );
	m_GraphicsSystem->showFps(true);	

	m_GraphicsSystem->run();	// runs in this (main) thread

	m_PhysicsSystem->terminate();
	m_shouldTerminate = true;
	m_LogicThread->join();
}
//////////////////////////////////////////////////////////////////////////
Engine & Engine::instance()
{
	static Poco::SingletonHolder<Engine> sh;
	return *sh.get();
}
//////////////////////////////////////////////////////////////////////////
Engine::~Engine()
{
	SAFE_DEL(m_GraphicsSystem);
	SAFE_DEL(m_PhysicsSystem);
	SAFE_DEL(m_GameStateSystem);
	SAFE_DEL(m_EventScheduler)
	SAFE_DEL(m_ComponentSystem)
	SAFE_DEL(m_logicDispatcher)
	SAFE_DEL(m_EntitySystem)

	SAFE_DEL(m_Application);
	SAFE_DEL(m_LogicThread);
	LogMgr::stop();
}
//////////////////////////////////////////////////////////////////////////
Engine::Engine() :
	m_Application(nullptr),
	m_EventScheduler(nullptr),
	m_GraphicsSystem(nullptr),
	m_PhysicsSystem(nullptr),
	m_GameStateSystem(nullptr),
	m_ComponentSystem(nullptr),
	m_EntitySystem(nullptr),
	m_LogicThread(nullptr),
	m_shouldTerminate(false),
	m_logicDispatcher(nullptr)
{
	m_LogicThread = new Poco::Thread();	
}
//////////////////////////////////////////////////////////////////////////