#include "Engine.h"
#include "LogMgr.h"
#include "EventSystem.h"
#include "GraphicsSystem.h"
#include "IPakalApplication.h"

#include "Poco/SingletonHolder.h"

using namespace Pakal;

bool Pakal::Engine::msInitialized = false;

void Pakal::Engine::run( IPakalApplication *application )
{
	Pakal::LogMgr::init();
	LOG_INFO("Initializing Pakal Engine Version " PAKAL_VERSION_NAME);

	mApplication = application;

	mGraphicsSystem = GraphicsSystem::createGraphicsSystem();
	mGraphicsSystem->init();
	mGraphicsSystem->initWindow();

	msInitialized = true;
}

Pakal::Engine & Pakal::Engine::instance()
{
	static Poco::SingletonHolder<Pakal::Engine> sh;
	return *sh.get();
}

Pakal::Engine::~Engine()
{
	SAFE_DEL(mGraphicsSystem);
	SAFE_DEL(mApplication);
}
