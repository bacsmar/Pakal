#include "Engine.h"
#include "LogMgr.h"
#include "GraphicsSystem.h"
#include "PhysicsSystem.h"
#include "GameStateSystem.h"
#include "IPakalApplication.h"
#include "EventSystem.h"

#include "IComponent.h"
#include "ComponentSystem.h"

#include "Poco/SingletonHolder.h"
#include "Poco/Thread.h"
#include "Poco/RunnableAdapter.h"


using namespace Pakal;
//////////////////////////////////////////////////////////////////////////
bool Engine::ms_Initialized = false;
//////////////////////////////////////////////////////////////////////////
void Engine::update()
{
	std::cout << "Hello, world! from engine" << std::endl;
}
//////////////////////////////////////////////////////////////////////////
void Engine::init()
{
	ASSERT(ms_Initialized == false);
	LOG_INFO("Initializing Pakal Engine Version " PAKAL_VERSION_NAME);			

	m_Application->setUpGameStates(m_GameStateSystem);

	// TODO: loop para el thread de logica, wait, sleep, wait for algo
	//m_PhysicsSystem->terminate();

	ms_Initialized = true;

	update();
}
//////////////////////////////////////////////////////////////////////////
void Engine::start( IPakalApplication *application )
{
	LogMgr::init();
	LogMgr::setLogLevel(10);
	m_Application = application;

	m_GraphicsSystem = GraphicsSystem::createGraphicsSystem();
	m_PhysicsSystem = PhysicsSystem::createPhysicsSystem();
	m_EventSystem = new EventSystem();
	m_GameStateSystem = new GameStateSystem();
	m_ComponentSystem = new ComponentSystem();

	m_GameStateSystem->initialize(this);
	m_PhysicsSystem->initialize();
	m_EventSystem->initialize();

	m_ComponentSystem->registerFactories(m_GraphicsSystem);

	Poco::RunnableAdapter<Engine> logic_entry_point(*this, &Engine::init);
	m_LogicThread->setName("Logic");
	m_LogicThread->start(logic_entry_point);

	m_GraphicsSystem->run();
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
	SAFE_DEL(m_EventSystem)

	SAFE_DEL(m_LogicThread);
}
//////////////////////////////////////////////////////////////////////////
Engine::Engine() :
	m_Application(nullptr),
	m_EventSystem(nullptr),
	m_GraphicsSystem(nullptr),
	m_GameStateSystem(nullptr),
	m_LogicThread(nullptr),
	m_PhysicsSystem(nullptr),
	m_ComponentSystem(nullptr),
	m_EntitySystem(nullptr)
{
	m_LogicThread = new Poco::Thread();
}
//////////////////////////////////////////////////////////////////////////