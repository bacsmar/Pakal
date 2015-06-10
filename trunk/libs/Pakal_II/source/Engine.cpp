#include "Engine.h"

#include "Poco/SingletonHolder.h"
#include "Poco/Logger.h"
#include "Poco/ConsoleChannel.h"
#include "Poco/WindowsConsoleChannel.h"

#include "LogMgr.h"

using namespace Pakal;

void Pakal::Engine::run( IPakalApplication *application )
{
	Pakal::LogMgr::init();

	LOG_INFO("Initializing Pakal Engine Version " PAKAL_VERSION_NAME);
	mApplication = application;
}

Engine & Pakal::Engine::instance()
{
	static Poco::SingletonHolder<Pakal::Engine> sh;
	return *sh.get();
}
