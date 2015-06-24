#include "Engine.h"
#include "LogMgr.h"
#include "GraphicsSystem.h"
#include "PhysicsSystem.h"
#include "GameStateSystem.h"
#include "IPakalApplication.h"
#include "EventScheduler.h"

#include "ComponentSystem.h"

#include "Poco/SingletonHolder.h"
#include "Poco/Thread.h"
#include "Poco/RunnableAdapter.h"

//////////////////////////// BEGIN TESTS /////////////////////////////////


//////////////////////////// END TESTS /////////////////////////////////

using namespace Pakal;


//////////////////////////////////////////////////////////////////////////
bool Engine::ms_Initialized = false;
//////////////////////////////////////////////////////////////////////////
void Engine::run()
{
	std::cout << "Hello, world! from engine" << std::endl;

}
//////////////////////////////////////////////////////////////////////////
void Engine::init()
{
	ASSERT(ms_Initialized == false);	

	m_Application->setUpGameStates(m_GameStateSystem);

	// TODO: loop para el thread de logica, wait, sleep, wait for algo	

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

	m_GraphicsSystem = GraphicsSystem::createGraphicsSystem();
	m_PhysicsSystem = PhysicsSystem::createPhysicsSystem();
	m_EventScheduler = new EventScheduler();
	m_GameStateSystem = new GameStateSystem();
	m_ComponentSystem = new ComponentSystem();

	m_ComponentSystem->registerFactories(m_GraphicsSystem);
	m_ComponentSystem->registerFactories(m_PhysicsSystem);

	m_EventScheduler->registerDispatcher(m_GraphicsSystem);
	m_EventScheduler->registerDispatcher(m_PhysicsSystem);

	m_GameStateSystem->initialize(this);
	m_PhysicsSystem->initialize();	// it creates his own thread

	Poco::RunnableAdapter<Engine> logic_entry_point(*this, &Engine::init);
	m_LogicThread->setName("Logic");
	m_LogicThread->start(logic_entry_point);

	// TODO
	//m_GraphicsSystem->addDebugDrawer( IDebugDrawer );
	//m_GraphicsSystem->addDebugDrawer( DebugDrawerDelegate );

	m_GraphicsSystem->run();	// runs in this (main) thread

	m_PhysicsSystem->terminate();
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
	SAFE_DEL(m_Application);
	SAFE_DEL(m_GameStateSystem);
	SAFE_DEL(m_EventScheduler)

	SAFE_DEL(m_LogicThread);
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
	m_LogicThread(nullptr)
{
	m_LogicThread = new Poco::Thread();
}
//////////////////////////////////////////////////////////////////////////

// TODO: DELETE THIS SHIT
namespace Pakal
{
	class IrrGraphicsSystem;
	const IrrGraphicsSystem * getIrrlicht()
	{	
		return reinterpret_cast<IrrGraphicsSystem*>( Engine::instance().getGraphicsSystem() );		
	}
}