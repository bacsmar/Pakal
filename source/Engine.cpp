#include "Engine.h"
#include "LogMgr.h"
#include "EventSystem.h"
#include "GraphicsSystem.h"
#include "IPakalApplication.h"

#include "Poco/SingletonHolder.h"

using namespace Pakal;

bool Pakal::Engine::ms_Initialized = false;

void Pakal::Engine::run( IPakalApplication *application )
{
	Pakal::LogMgr::init();
	LOG_INFO("Initializing Pakal Engine Version " PAKAL_VERSION_NAME);

	m_Application = application;

	m_GraphicsSystem = GraphicsSystem::createGraphicsSystem();
	m_GraphicsSystem->init();
	m_GraphicsSystem->initWindow();

	ms_Initialized = true;

	// TODO: Arrancar los threads

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
		bool running = m_GraphicsSystem->update();

		if (msg.message == WM_QUIT || !running)
		{
			// TODO: notificar a los sistemas que hemos terminado 
			break;
		}
	}
}

Pakal::Engine & Pakal::Engine::instance()
{
	static Poco::SingletonHolder<Pakal::Engine> sh;
	return *sh.get();
}

Pakal::Engine::~Engine()
{
	SAFE_DEL(m_GraphicsSystem);
	SAFE_DEL(m_Application);
}
