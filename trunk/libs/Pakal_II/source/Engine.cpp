#include "Engine.h"

#include "Poco/SingletonHolder.h"
#include "LogMgr.h"

void Pakal::Engine::run( IPakalApplication *application )
{
	Pakal::LogMgr::init();
	LOG_INFO("Initializing Pakal Engine Version " PAKAL_VERSION_NAME);
	mApplication = application;
}

Pakal::Engine & Pakal::Engine::instance()
{
	static Poco::SingletonHolder<Pakal::Engine> sh;
	return *sh.get();
}
