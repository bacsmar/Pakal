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
