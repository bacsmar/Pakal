#include "Engine.h"
#include "LogMgr.h"
#include "GraphicsSystem.h"
#include "PhysicsSystem.h"
#include "GameStateSystem.h"
#include "IPakalApplication.h"
#include "EventSystem.h"

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

	m_PhysicsSystem = PhysicsSystem::createPhysicsSystem();
	m_EventSystem = new EventSystem();
	m_GameStateSystem = new GameStateSystem();

	m_GameStateSystem->initialize(this);
	m_PhysicsSystem->initialize();
	m_EventSystem->initialize();

	ms_Initialized = true;

	m_PhysicsSystem->terminate();

	update();
}
//////////////////////////////////////////////////////////////////////////
void Engine::start( IPakalApplication *application )
{
	LogMgr::init();
	m_Application = application;

	m_GraphicsSystem = GraphicsSystem::createGraphicsSystem();

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
	m_LogicThread(nullptr)
{
	m_LogicThread = new Poco::Thread();
}
//////////////////////////////////////////////////////////////////////////