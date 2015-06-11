#include "Engine.h"
#include "LogMgr.h"
#include "EventSystem.h"
#include "GraphicsSystem.h"
#include "PhysicsSystem.h"
#include "IPakalApplication.h"

#include "Poco/SingletonHolder.h"
#include "Poco/Thread.h"
#include "Poco/RunnableAdapter.h"


using namespace Pakal;
//////////////////////////////////////////////////////////////////////////
bool Pakal::Engine::ms_Initialized = false;
//////////////////////////////////////////////////////////////////////////
void Pakal::Engine::update()
{
	std::cout << "Hello, world!" << std::endl;
}
//////////////////////////////////////////////////////////////////////////
void Pakal::Engine::init()
{
	ASSERT(ms_Initialized == false);

	Pakal::LogMgr::init();
	LOG_INFO("Initializing Pakal Engine Version " PAKAL_VERSION_NAME);

	m_GraphicsSystem = GraphicsSystem::createGraphicsSystem();
	m_PhysicsSystem = PhysicsSystem::createPhysicsSystem();

	m_GraphicsSystem->init();
	m_GraphicsSystem->initWindow();

	ms_Initialized = true;
}
//////////////////////////////////////////////////////////////////////////
void Pakal::Engine::run( IPakalApplication *application )
{
	m_Application = application;
	init();

	// we are using Engine::Update in the logic Thread, cause we need the graphics in the main thread
	Poco::RunnableAdapter<Pakal::Engine> logicRunnable(*this, &Pakal::Engine::update);

	Poco::RunnableAdapter<Pakal::PhysicsSystem> physicsRunnable(*m_PhysicsSystem, &Pakal::PhysicsSystem::run);

	m_LogicThread->setName("Logic");
	m_LogicThread->start(logicRunnable);

	m_PhysicsThread->setName("Physics");
	m_PhysicsThread->start(physicsRunnable);

	while( true )
	{		  

#ifdef PAKAL_WIN32_PLATFORM
		MSG  msg;
		while( PeekMessage( &msg, NULL, 0U, 0U, PM_REMOVE ) )
		{
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
#elif defined(PAKAL_ANDROID_PLATFORM)
		//ALooper_pollAll( EngineApp::getSingleton().isRunning() ? 0 : -1, NULL, NULL, NULL);
#endif
		// in Android the GraphicsSystem should run in the main thread...
		bool running = m_GraphicsSystem->update();

		if (msg.message == WM_QUIT || !running)
		{
			// TODO: notificar a los sistemas que hemos terminado 
			break;
		}
	}

	m_LogicThread->join();
	m_PhysicsThread->join();
}
//////////////////////////////////////////////////////////////////////////
Pakal::Engine & Pakal::Engine::instance()
{
	static Poco::SingletonHolder<Pakal::Engine> sh;
	return *sh.get();
}
//////////////////////////////////////////////////////////////////////////
Pakal::Engine::~Engine()
{
	SAFE_DEL(m_GraphicsSystem);
	SAFE_DEL(m_Application);

	SAFE_DEL(m_PhysicsThread);
	SAFE_DEL(m_LogicThread);
}
//////////////////////////////////////////////////////////////////////////
Pakal::Engine::Engine() :
	m_Application(0),
	m_EventSystem(0),
	m_GraphicsSystem(0),
	m_LogicThread(0),
	m_PhysicsThread(0)
{
	m_LogicThread = new Poco::Thread();
	m_PhysicsThread = new Poco::Thread();
}
//////////////////////////////////////////////////////////////////////////