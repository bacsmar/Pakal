#include "Engine.h"

#include "LogMgr.h"
#include "EventSystem.h"

#include "Poco/SingletonHolder.h"

#include "GraphicsSystem.h"

bool Pakal::Engine::msInitialized = false;

void Pakal::Engine::run( IPakalApplication *application )
{
	Pakal::LogMgr::init();
	LOG_INFO("Initializing Pakal Engine Version " PAKAL_VERSION_NAME);

	mApplication = application;

	mGraphicsSystem = GraphicsSystem::createGraphicsSystem();

	msInitialized = true;
}

Pakal::Engine & Pakal::Engine::instance()
{
	static Poco::SingletonHolder<Pakal::Engine> sh;
	return *sh.get();
}
